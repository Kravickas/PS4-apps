#include <stdint.h>
#include "pm4.h"

extern "C" {
    int  sceKernelAllocateDirectMemory(long a, long b, unsigned long c, unsigned long d, int e, long* f);
    int  sceKernelMapDirectMemory(void** a, unsigned long b, int c, int d, long e, unsigned long f);
    int  sceKernelUsleep(unsigned int a);
    int  sceGnmMapComputeQueue(uint32_t a, uint32_t b, uintptr_t c, uint32_t d, uint32_t* e);
    void sceGnmDingDong(uint32_t a, uint32_t b);
    int  sceGnmUnmapComputeQueue(uint32_t a);
    int  sceGnmSubmitDone(void);
    int  printf(const char* fmt, ...);
    int  sceKernelSendNotificationRequest(int device, void* req, unsigned long size, int blocking);
    int  sceKernelOpen(const char* path, int flags, int mode);
    long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int  sceKernelClose(int fd);
}

static void my_memset(void* d, int v, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) ((unsigned char*)d)[i] = (unsigned char)v;
}

struct NotifyRequest {
    char header[45];
    char message[3075];
};

static void notify(const char* msg) {
    NotifyRequest req;
    my_memset(&req, 0, sizeof(req));
    int i = 0;
    while (msg[i] && i < 3074) {
        req.message[i] = msg[i];
        i++;
    }
    req.message[i] = 0;
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

static int s_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void s_cat(char* dst, int* p, const char* s) {
    while (*s) dst[(*p)++] = *s++;
    dst[*p] = 0;
}

static void s_hex(char* dst, int* p, uint32_t v) {
    const char* H = "0123456789ABCDEF";
    dst[(*p)++] = '0';
    dst[(*p)++] = 'x';
    for (int i = 28; i >= 0; i -= 4) dst[(*p)++] = H[(v >> i) & 0xF];
    dst[*p] = 0;
}

static void s_hex64(char* dst, int* p, uint64_t v) {
    s_hex(dst, p, (uint32_t)(v >> 32));
    dst[(*p)++] = '_';
    s_hex(dst, p, (uint32_t)v);
    dst[*p] = 0;
}

// O_WRONLY|O_CREAT|O_TRUNC = 0x601, mode 0777
static const char* write_result_file(const char* text) {
    static const char* paths[2] = { "/data/img_mip_result.txt", "/mnt/usb0/img_mip_result.txt" };
    for (int i = 0; i < 2; i++) {
        int fd = sceKernelOpen(paths[i], 0x601, 0777);
        if (fd >= 0) {
            sceKernelWrite(fd, text, s_len(text));
            sceKernelClose(fd);
            return paths[i];
        }
    }
    return nullptr;
}

static void* gpu_alloc(unsigned long size, unsigned long align) {
    long phys = 0; void* addr = nullptr;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, 3, &phys)) return nullptr;
    if (sceKernelMapDirectMemory(&addr, size, 0x33, 0, phys, align)) return nullptr;
    my_memset(addr, 0, size);
    return addr;
}

struct ComputeQueue {
    uint32_t* ring; uint32_t* read_ptr; int vqid; uint32_t write_off;
    bool init() {
        ring = (uint32_t*)gpu_alloc(0x10000, 0x10000);
        read_ptr = (uint32_t*)gpu_alloc(0x10000, 0x10000);
        if (!ring || !read_ptr) return false;
        *read_ptr = 0; write_off = 0;
        vqid = sceGnmMapComputeQueue(0, 0, (uintptr_t)ring, 0x4000, read_ptr);
        return vqid > 0;
    }
    void submit(const PM4Builder& pm4) {
        for (uint32_t i = 0; i < pm4.off; i++) ring[write_off++] = pm4.buf[i];
        sceGnmDingDong((uint32_t)vqid, write_off);
    }
    void destroy() { if (vqid > 0) sceGnmUnmapComputeQueue((uint32_t)vqid); }
};

// ================================================================
// Write shader — T# passed directly in s[0:7] via USER_DATA
// No s_load needed — avoids SRT pass entirely
// ================================================================
static const uint32_t shader_write[] = {
    // s[0:7] = T# from COMPUTE_USER_DATA_0..7
    0x7E020080,             // v_mov_b32 v0, 0          ; x
    0x7E020280,             // v_mov_b32 v1, 0          ; y
    0x7E020480,             // v_mov_b32 v2, 0          ; fragid=0
    0x7E0208FF, 0xAAAAAAAA, // v_mov_b32 v4, 0xAAAAAAAA
    0xF0201100, 0x00000400, // image_store v4, v[0:2], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0x7E020481,             // v_mov_b32 v2, 1          ; fragid=1
    0x7E0208FF, 0xBBBBBBBB, // v_mov_b32 v4, 0xBBBBBBBB
    0xF0201100, 0x00000400, // image_store v4, v[0:2], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xBF810000,             // s_endpgm
    // BinaryInfo (7 DW)
    0x5362724F, 0x00726468, // "OrbShdr" + version=0
    0x00005814,             // type=5(CS), length=92
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

// ================================================================
// Read shader — T# in s[0:7], V# in s[8:11] via USER_DATA
// ================================================================
static const uint32_t shader_read[] = {
    // s[0:7]=T#, s[8:11]=V# from COMPUTE_USER_DATA

    // Test 0: IMAGE_LOAD fragid=0
    0x7E020080,             // v_mov_b32 v0, 0
    0x7E020280,             // v_mov_b32 v1, 0
    0x7E020480,             // v_mov_b32 v2, 0          ; fragid=0
    0xF0001100, 0x00000A00, // image_load v10, v[0:2], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE0700000, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:0

    // Test 1: IMAGE_LOAD fragid=1
    0x7E020481,             // v_mov_b32 v2, 1
    0xF0001100, 0x00000A00, // image_load v10, v[0:2], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE0700004, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:4

    // Test 2: IMAGE_LOAD_MIP v2=0, v3=1
    0x7E020080,             // v_mov_b32 v0, 0
    0x7E020280,             // v_mov_b32 v1, 0
    0x7E020480,             // v_mov_b32 v2, 0
    0x7E020681,             // v_mov_b32 v3, 1
    0xF0041100, 0x00000A00, // image_load_mip v10, v[0:3], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE0700008, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:8

    // Test 3: IMAGE_LOAD_MIP v2=1, v3=0
    0x7E020481,             // v_mov_b32 v2, 1
    0x7E020680,             // v_mov_b32 v3, 0
    0xF0041100, 0x00000A00, // image_load_mip v10, v[0:3], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE070000C, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:12

    // Test 4: IMAGE_LOAD_MIP v2=1, v3=1
    0x7E020481,             // v_mov_b32 v2, 1
    0x7E020681,             // v_mov_b32 v3, 1
    0xF0041100, 0x00000A00, // image_load_mip v10, v[0:3], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE0700010, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:16

    // Test 5: IMAGE_LOAD_MIP v2=0, v3=0
    0x7E020480,             // v_mov_b32 v2, 0
    0x7E020680,             // v_mov_b32 v3, 0
    0xF0041100, 0x00000A00, // image_load_mip v10, v[0:3], s[0:7] dmask:1 unorm
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xE0700014, 0x80020A00, // buffer_store_dword v10, off, s[8:11], 0x80 offset:20

    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xBF810000,             // s_endpgm
    // BinaryInfo (7 DW)
    0x5362724F, 0x00726468, // "OrbShdr" + version=0
    0x0000D414,             // type=5(CS), length=208
    0x00000000, 0x00000000, 0x00000001, 0x00000000,
};

// T# for 2D MSAA
static void build_t_sharp(uint32_t d[8], uint64_t addr) {
    my_memset(d, 0, 32);
    uint64_t base = addr >> 8;
    uint64_t w01 = (base & 0x3FFFFFFFFFull) | (4ull << 52) | (4ull << 58);
    d[0] = (uint32_t)w01; d[1] = (uint32_t)(w01 >> 32);
    uint64_t w23 = 7ull | (7ull << 14) | (4ull << 32) | (5ull << 35) |
        (6ull << 38) | (7ull << 41) | (1ull << 48) | (14ull << 52) | (14ull << 60);
    d[2] = (uint32_t)w23; d[3] = (uint32_t)(w23 >> 32);
    d[4] = (uint32_t)(7ull << 13); // pitch=8-1
}

// V# for output buffer
static void build_v_sharp(uint32_t d[4], uint64_t addr, uint32_t sz) {
    d[0] = (uint32_t)(addr); d[1] = (uint32_t)((addr >> 32) & 0xFFF);
    d[2] = sz; d[3] = 0;
}

static void dispatch_write(ComputeQueue& cq, uint64_t shader_addr,
                          uint32_t t_sharp[8], volatile uint32_t* fence) {
    uint32_t pm4_buf[128];
    PM4Builder pm4 = { pm4_buf, 0 };

    // RSRC1: vgprs=1(8), sgprs=0(8), float=0xC0, dx10_clamp
    uint32_t rsrc1 = 1u | (0u << 6) | (0xC0u << 12) | (1u << 21);
    // RSRC2: user_sgpr=8 (s[0:7] for T#)
    uint32_t rsrc2 = (8u << 1);

    pm4.set_sh_reg2(mmCOMPUTE_PGM_LO, (uint32_t)(shader_addr >> 8), (uint32_t)(shader_addr >> 40));
    pm4.set_sh_reg2(mmCOMPUTE_PGM_RSRC1, rsrc1, rsrc2);
    pm4.set_sh_reg3(mmCOMPUTE_NUM_THREAD_X, 1, 1, 1);

    // Pass T# directly in USER_DATA_0..7
    pm4.emit(PM4_HDR(PM4_SET_SH_REG, 9));
    pm4.emit(SH(mmCOMPUTE_USER_DATA_0));
    for (int i = 0; i < 8; i++) pm4.emit(t_sharp[i]);

    pm4.dispatch(1, 1, 1);
    pm4.release_mem((uint64_t)fence, 1);

    *fence = 0;
    cq.submit(pm4);
    sceGnmSubmitDone();
    int t = 0;
    while (*fence == 0 && t < 50000) { sceKernelUsleep(100); t++; }
}

static void dispatch_read(ComputeQueue& cq, uint64_t shader_addr,
                         uint32_t t_sharp[8], uint32_t v_sharp[4],
                         volatile uint32_t* fence) {
    uint32_t pm4_buf[128];
    PM4Builder pm4 = { pm4_buf, 0 };

    // RSRC1: vgprs=2(12), sgprs=1(16), float=0xC0, dx10_clamp
    uint32_t rsrc1 = 2u | (1u << 6) | (0xC0u << 12) | (1u << 21);
    // RSRC2: user_sgpr=12 (s[0:7]=T#, s[8:11]=V#)
    uint32_t rsrc2 = (12u << 1);

    pm4.set_sh_reg2(mmCOMPUTE_PGM_LO, (uint32_t)(shader_addr >> 8), (uint32_t)(shader_addr >> 40));
    pm4.set_sh_reg2(mmCOMPUTE_PGM_RSRC1, rsrc1, rsrc2);
    pm4.set_sh_reg3(mmCOMPUTE_NUM_THREAD_X, 1, 1, 1);

    // Pass T# in USER_DATA_0..7, V# in USER_DATA_8..11
    pm4.emit(PM4_HDR(PM4_SET_SH_REG, 13));
    pm4.emit(SH(mmCOMPUTE_USER_DATA_0));
    for (int i = 0; i < 8; i++) pm4.emit(t_sharp[i]);
    for (int i = 0; i < 4; i++) pm4.emit(v_sharp[i]);

    pm4.dispatch(1, 1, 1);
    pm4.release_mem((uint64_t)fence, 1);

    *fence = 0;
    cq.submit(pm4);
    sceGnmSubmitDone();
    int t = 0;
    while (*fence == 0 && t < 50000) { sceKernelUsleep(100); t++; }
}

int main() {
    printf("=== IMAGE_LOAD_MIP + MSAA T# HW TEST ===\n\n");

    void* img_mem = gpu_alloc(0x10000, 0x10000);
    volatile uint32_t* out_buf = (volatile uint32_t*)gpu_alloc(0x10000, 0x10000);
    uint8_t* shd_mem = (uint8_t*)gpu_alloc(0x10000, 0x10000);
    volatile uint32_t* fence = (volatile uint32_t*)gpu_alloc(0x10000, 0x10000);

    if (!img_mem || !out_buf || !shd_mem || !fence) {
        printf("FAIL: alloc\n");
        notify("IMG_MIP: FAIL alloc");
        return 1;
    }

    // Copy shaders
    for (unsigned i = 0; i < sizeof(shader_write)/4; i++)
        ((uint32_t*)shd_mem)[i] = shader_write[i];
    for (unsigned i = 0; i < sizeof(shader_read)/4; i++)
        ((uint32_t*)(shd_mem + 0x200))[i] = shader_read[i];

    // Build descriptors
    uint32_t t_sharp[8], v_sharp[4];
    build_t_sharp(t_sharp, (uint64_t)img_mem);
    build_v_sharp(v_sharp, (uint64_t)out_buf, 64);

    printf("img=%p out=%p shd=%p\n", img_mem, (void*)out_buf, shd_mem);

    ComputeQueue cq;
    if (!cq.init()) {
        printf("FAIL: CQ\n");
        notify("IMG_MIP: FAIL compute queue");
        return 1;
    }
    printf("CQ vqid=%d\n\n", cq.vqid);
    notify("IMG_MIP: test running");

    printf("Phase 1: Write...\n");
    dispatch_write(cq, (uint64_t)shd_mem, t_sharp, fence);
    uint32_t wfence = *fence;
    printf("  done (fence=%u)\n\n", wfence);

    printf("Phase 2: Read...\n");
    for (int i = 0; i < 16; i++) ((volatile uint32_t*)out_buf)[i] = 0xDDDDDDDD;
    dispatch_read(cq, (uint64_t)shd_mem + 0x200, t_sharp, v_sharp, fence);
    uint32_t rfence = *fence;
    printf("  done (fence=%u)\n\n", rfence);

    printf("=== RESULTS ===\n");
    printf("[0] LOAD     frag=0 : 0x%08X\n", out_buf[0]);
    printf("[1] LOAD     frag=1 : 0x%08X\n", out_buf[1]);
    printf("[2] LOAD_MIP v2=0,v3=1: 0x%08X\n", out_buf[2]);
    printf("[3] LOAD_MIP v2=1,v3=0: 0x%08X\n", out_buf[3]);
    printf("[4] LOAD_MIP v2=1,v3=1: 0x%08X\n", out_buf[4]);
    printf("[5] LOAD_MIP v2=0,v3=0: 0x%08X\n", out_buf[5]);

    // Build the result text (sent back for analysis)
    char ft[1024];
    int fp = 0;
    s_cat(ft, &fp, "IMAGE_LOAD_MIP + MSAA T# HW TEST\n");
    s_cat(ft, &fp, "verdict: ");
    if (out_buf[2]==0xAAAAAAAA && out_buf[3]==0xBBBBBBBB)
        s_cat(ft, &fp, "IGNORES _MIP (v2=fragid)\n");
    else if (out_buf[2]==0xBBBBBBBB && out_buf[3]==0xAAAAAAAA)
        s_cat(ft, &fp, "KEEPS _MIP (v3=fragid)\n");
    else if (out_buf[0]==0xDDDDDDDD)
        s_cat(ft, &fp, "NO DATA - T#/tiling\n");
    else
        s_cat(ft, &fp, "UNEXPECTED\n");
    s_cat(ft, &fp, "[0] LOAD     frag0     = "); s_hex(ft, &fp, out_buf[0]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "[1] LOAD     frag1     = "); s_hex(ft, &fp, out_buf[1]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "[2] LOAD_MIP v2=0,v3=1 = "); s_hex(ft, &fp, out_buf[2]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "[3] LOAD_MIP v2=1,v3=0 = "); s_hex(ft, &fp, out_buf[3]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "[4] LOAD_MIP v2=1,v3=1 = "); s_hex(ft, &fp, out_buf[4]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "[5] LOAD_MIP v2=0,v3=0 = "); s_hex(ft, &fp, out_buf[5]); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "write_fence = "); s_hex(ft, &fp, wfence); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "read_fence  = "); s_hex(ft, &fp, rfence); s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "raw out[0..15]:");
    for (int i = 0; i < 16; i++) { s_cat(ft, &fp, " "); s_hex(ft, &fp, out_buf[i]); }
    s_cat(ft, &fp, "\n");
    s_cat(ft, &fp, "img="); s_hex64(ft, &fp, (uint64_t)img_mem);
    s_cat(ft, &fp, " out="); s_hex64(ft, &fp, (uint64_t)out_buf);
    s_cat(ft, &fp, " shd="); s_hex64(ft, &fp, (uint64_t)shd_mem);
    s_cat(ft, &fp, "\n");

    const char* wrote = write_result_file(ft);

    // On-screen notification: verdict + where the file landed
    char msg[256];
    int mp = 0;
    if (out_buf[2]==0xAAAAAAAA && out_buf[3]==0xBBBBBBBB)
        s_cat(msg, &mp, "IGNORES _MIP (v2=fragid)\n");
    else if (out_buf[2]==0xBBBBBBBB && out_buf[3]==0xAAAAAAAA)
        s_cat(msg, &mp, "KEEPS _MIP (v3=fragid)\n");
    else if (out_buf[0]==0xDDDDDDDD)
        s_cat(msg, &mp, "NO DATA - T#/tiling\n");
    else
        s_cat(msg, &mp, "UNEXPECTED\n");
    s_cat(msg, &mp, "M2="); s_hex(msg, &mp, out_buf[2]);
    s_cat(msg, &mp, " M3="); s_hex(msg, &mp, out_buf[3]);
    s_cat(msg, &mp, "\n");
    if (wrote) {
        s_cat(msg, &mp, "saved: ");
        s_cat(msg, &mp, wrote);
    } else {
        s_cat(msg, &mp, "FILE WRITE FAILED");
    }
    notify(msg);

    printf("\nDone.\n");
    cq.destroy();
    sceKernelUsleep(10000000);
    return 0;
}
