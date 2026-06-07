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
    int  sceKernelOpen(const char* path, int flags, int mode);
    long sceKernelWrite(int fd, const void* buf, unsigned long n);
    int  sceKernelClose(int fd);
    int  printf(const char* fmt, ...);
}

static void my_memset(void* d, int v, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) ((unsigned char*)d)[i] = (unsigned char)v;
}

// WB_ONION (type 0): CPU-coherent, so shader code and results are visible across CPU/GPU
// without manual cache management. The image uses the same pool; tiling comes from the T#,
// not the memory type, so store and read stay consistent as long as they share the T#.
static void* gpu_alloc(unsigned long size, unsigned long align) {
    long phys = 0; void* addr = nullptr;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, 0, &phys)) return nullptr;
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
// Write shader — T# in s[0:7]. Stores 0xAAAAAAAA to sample 0 and
// 0xBBBBBBBB to sample 1 of the MSAA image via IMAGE_STORE (fragid in v2).
// ================================================================
static const uint32_t shader_write[] = {
    0x7E020080,             // v_mov_b32 v0, 0          ; x
    0x7E020280,             // v_mov_b32 v1, 0          ; y
    0x7E020480,             // v_mov_b32 v2, 0          ; fragid=0
    0x7E0208FF, 0xAAAAAAAA, // v_mov_b32 v4, 0xAAAAAAAA
    0xF0201100, 0x00000400, // image_store v4, v[0:2], s[0:7] dmask:1
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0x7E020481,             // v_mov_b32 v2, 1          ; fragid=1
    0x7E0208FF, 0xBBBBBBBB, // v_mov_b32 v4, 0xBBBBBBBB
    0xF0201100, 0x00000400, // image_store v4, v[0:2], s[0:7] dmask:1
    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xBF810000,             // s_endpgm
    0x5362724F, 0x00726468, // "OrbShdr" + version=0
    0x00005814,             // type=5(CS), length=92
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

// ================================================================
// Read shader — T# in s[0:7], V# in s[8:11].
// Smoke store first (no image) proves the wave launches and the store
// path works. Then baseline IMAGE_LOAD (fragid in v2) and the CTR case
// IMAGE_LOAD_MIP (op=1, 0xf0040100) varying v2/v3 to discriminate
// Outcome A (v2 is the sample) from Outcome B (v3 is the sample).
// ================================================================
static const uint32_t shader_read[] = {
    // Smoke: store a constant, no image touched -> out[6]
    0x7E1402FF, 0xC0DE0001, // v_mov_b32 v10, 0xC0DE0001
    0xE0700018, 0x80020A00, // buffer_store_dword v10, off, s[8:11], offset:24
    0xBF8C1F70,             // s_waitcnt vmcnt(0)

    // Test 0: IMAGE_LOAD fragid=0
    0x7E020080,             // v_mov_b32 v0, 0
    0x7E020280,             // v_mov_b32 v1, 0
    0x7E020480,             // v_mov_b32 v2, 0
    0xF0001100, 0x00000A00, // image_load v10, v[0:2], s[0:7] dmask:1
    0xBF8C1F70,
    0xE0700000, 0x80020A00, // offset:0

    // Test 1: IMAGE_LOAD fragid=1
    0x7E020481,             // v_mov_b32 v2, 1
    0xF0001100, 0x00000A00,
    0xBF8C1F70,
    0xE0700004, 0x80020A00, // offset:4

    // Test 2: IMAGE_LOAD_MIP v2=0, v3=1   (CTR opcode 0xf0040100)
    0x7E020080,             // v_mov_b32 v0, 0
    0x7E020280,             // v_mov_b32 v1, 0
    0x7E020480,             // v_mov_b32 v2, 0
    0x7E020681,             // v_mov_b32 v3, 1
    0xF0040100, 0x00000A00, // image_load_mip v10, v[0:3], s[0:7] dmask:1
    0xBF8C1F70,
    0xE0700008, 0x80020A00, // offset:8

    // Test 3: IMAGE_LOAD_MIP v2=1, v3=0
    0x7E020481,             // v_mov_b32 v2, 1
    0x7E020680,             // v_mov_b32 v3, 0
    0xF0040100, 0x00000A00,
    0xBF8C1F70,
    0xE070000C, 0x80020A00, // offset:12

    // Test 4: IMAGE_LOAD_MIP v2=1, v3=1
    0x7E020481,             // v_mov_b32 v2, 1
    0x7E020681,             // v_mov_b32 v3, 1
    0xF0040100, 0x00000A00,
    0xBF8C1F70,
    0xE0700010, 0x80020A00, // offset:16

    // Test 5: IMAGE_LOAD_MIP v2=0, v3=0  (CTR's exact case: mipid 0)
    0x7E020480,             // v_mov_b32 v2, 0
    0x7E020680,             // v_mov_b32 v3, 0
    0xF0040100, 0x00000A00,
    0xBF8C1F70,
    0xE0700014, 0x80020A00, // offset:20

    0xBF8C1F70,             // s_waitcnt vmcnt(0)
    0xBF810000,             // s_endpgm
    0x5362724F, 0x00726468, // "OrbShdr" + version=0
    0x0000F014,             // type=5(CS)
    0x00000000, 0x00000000, 0x00000001, 0x00000000,
};

// T# for 2D MSAA (type=14, last_level=1 -> 2 samples, R32_UINT, 8x8)
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

    uint32_t rsrc1 = 1u | (0u << 6) | (0xC0u << 12) | (1u << 21);
    uint32_t rsrc2 = (8u << 1);

    pm4.set_sh_reg2(mmCOMPUTE_PGM_LO, (uint32_t)(shader_addr >> 8), (uint32_t)(shader_addr >> 40));
    pm4.set_sh_reg2(mmCOMPUTE_PGM_RSRC1, rsrc1, rsrc2);
    pm4.set_sh_reg3(mmCOMPUTE_NUM_THREAD_X, 1, 1, 1);
    pm4.set_sh_reg(mmCOMPUTE_RESOURCE_LIMITS, 0);
    pm4.set_sh_reg2(mmCOMPUTE_STATIC_THREAD_MGMT_SE0, 0xFFFFFFFF, 0xFFFFFFFF);

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

    uint32_t rsrc1 = 2u | (1u << 6) | (0xC0u << 12) | (1u << 21);
    uint32_t rsrc2 = (12u << 1);

    pm4.set_sh_reg2(mmCOMPUTE_PGM_LO, (uint32_t)(shader_addr >> 8), (uint32_t)(shader_addr >> 40));
    pm4.set_sh_reg2(mmCOMPUTE_PGM_RSRC1, rsrc1, rsrc2);
    pm4.set_sh_reg3(mmCOMPUTE_NUM_THREAD_X, 1, 1, 1);
    pm4.set_sh_reg(mmCOMPUTE_RESOURCE_LIMITS, 0);
    pm4.set_sh_reg2(mmCOMPUTE_STATIC_THREAD_MGMT_SE0, 0xFFFFFFFF, 0xFFFFFFFF);

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

static void s_cat(char* d, int* p, const char* s) { int i = 0; while (s[i]) d[(*p)++] = s[i++]; }
static void s_hex(char* d, int* p, uint32_t v) {
    const char* h = "0123456789ABCDEF";
    s_cat(d, p, "0x");
    for (int i = 7; i >= 0; i--) d[(*p)++] = h[(v >> (i * 4)) & 0xF];
}
static void s_hex64(char* d, int* p, uint64_t v) {
    s_hex(d, p, (uint32_t)(v >> 32)); d[(*p)++] = '_'; s_hex(d, p, (uint32_t)v);
}

static void write_result(volatile uint32_t* out, uint32_t wf, uint32_t rf,
                         uint64_t img, uint64_t outa, uint64_t shd) {
    char b[4096]; int p = 0;
    s_cat(b, &p, "IMAGE_LOAD_MIP + MSAA T# HW TEST\n");
    s_cat(b, &p, "[0] LOAD     frag0     = "); s_hex(b, &p, out[0]); s_cat(b, &p, "\n");
    s_cat(b, &p, "[1] LOAD     frag1     = "); s_hex(b, &p, out[1]); s_cat(b, &p, "\n");
    s_cat(b, &p, "[2] LOAD_MIP v2=0,v3=1 = "); s_hex(b, &p, out[2]); s_cat(b, &p, "\n");
    s_cat(b, &p, "[3] LOAD_MIP v2=1,v3=0 = "); s_hex(b, &p, out[3]); s_cat(b, &p, "\n");
    s_cat(b, &p, "[4] LOAD_MIP v2=1,v3=1 = "); s_hex(b, &p, out[4]); s_cat(b, &p, "\n");
    s_cat(b, &p, "[5] LOAD_MIP v2=0,v3=0 = "); s_hex(b, &p, out[5]); s_cat(b, &p, "\n");
    s_cat(b, &p, "write_fence = "); s_hex(b, &p, wf); s_cat(b, &p, "\n");
    s_cat(b, &p, "read_fence  = "); s_hex(b, &p, rf); s_cat(b, &p, "\n");
    s_cat(b, &p, "smoke out[6] = "); s_hex(b, &p, out[6]); s_cat(b, &p, "\n");
    s_cat(b, &p, "verdict: ");
    if (out[6] != 0xC0DE0001)
        s_cat(b, &p, "WAVE DID NOT RUN (smoke store failed)\n");
    else if (out[0] == 0xDDDDDDDD || out[1] == 0xDDDDDDDD)
        s_cat(b, &p, "WAVE RAN, image read failed (T#/tiling)\n");
    else if (out[2] == 0xAAAAAAAA && out[3] == 0xBBBBBBBB)
        s_cat(b, &p, "OUTCOME A - v2 (mip slot) is the sample; v3 ignored\n");
    else if (out[2] == 0xBBBBBBBB && out[3] == 0xAAAAAAAA)
        s_cat(b, &p, "OUTCOME B - v3 is the sample\n");
    else
        s_cat(b, &p, "UNEXPECTED\n");
    s_cat(b, &p, "img="); s_hex64(b, &p, img);
    s_cat(b, &p, " out="); s_hex64(b, &p, outa);
    s_cat(b, &p, " shd="); s_hex64(b, &p, shd); s_cat(b, &p, "\n");

    int fd = sceKernelOpen("/data/img_mip_result.txt", 0x601, 0x1B6);
    if (fd < 0) fd = sceKernelOpen("/mnt/usb0/img_mip_result.txt", 0x601, 0x1B6);
    if (fd >= 0) { sceKernelWrite(fd, b, (unsigned long)p); sceKernelClose(fd); }
    printf("%s", b);
}

int main() {
    void* img_mem = gpu_alloc(0x10000, 0x10000);
    volatile uint32_t* out_buf = (volatile uint32_t*)gpu_alloc(0x10000, 0x10000);
    uint8_t* shd_mem = (uint8_t*)gpu_alloc(0x10000, 0x10000);
    volatile uint32_t* fence = (volatile uint32_t*)gpu_alloc(0x10000, 0x10000);
    if (!img_mem || !out_buf || !shd_mem || !fence) { printf("FAIL: alloc\n"); return 1; }

    for (unsigned i = 0; i < sizeof(shader_write) / 4; i++)
        ((uint32_t*)shd_mem)[i] = shader_write[i];
    for (unsigned i = 0; i < sizeof(shader_read) / 4; i++)
        ((uint32_t*)(shd_mem + 0x200))[i] = shader_read[i];

    uint32_t t_sharp[8], v_sharp[4];
    build_t_sharp(t_sharp, (uint64_t)img_mem);
    build_v_sharp(v_sharp, (uint64_t)out_buf, 64);

    ComputeQueue cq;
    if (!cq.init()) { printf("FAIL: CQ\n"); return 1; }

    dispatch_write(cq, (uint64_t)shd_mem, t_sharp, fence);
    uint32_t wf = *fence;

    for (int i = 0; i < 16; i++) ((volatile uint32_t*)out_buf)[i] = 0xDDDDDDDD;
    dispatch_read(cq, (uint64_t)shd_mem + 0x200, t_sharp, v_sharp, fence);
    uint32_t rf = *fence;

    write_result(out_buf, wf, rf, (uint64_t)img_mem, (uint64_t)out_buf, (uint64_t)shd_mem);

    cq.destroy();
    sceKernelUsleep(10000000);
    return 0;
}
