/* IB chain hardware test — Liverpool GFX7 ground truth. Standalone OpenOrbis eboot.
 *
 * Pure CP packet-parsing test: only WRITE_DATA and INDIRECT_BUFFER.
 * No shaders, no draws, no state, no EOP.
 *
 * Encodings verified against shadPS4 pm4_cmds.h / liverpool.cpp, PAL
 * si_ci_vi_merged_pm4defs.h (GFX7/CI field layout) and gfx6CmdUtil.cpp:1983
 * (chain semantics), and libSceGnmDriver.sprx 12.02 (submitted DCB is IB1).
 * The IB dword builder reproduces real RDR2 packet dwords bit-for-bit.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ---- OpenOrbis SDK (exact prototypes from include/orbis) ------------------ */
typedef int64_t off_t;

extern int32_t sceKernelAllocateDirectMemory(off_t start, off_t end, size_t len, size_t align,
                                             int32_t type, off_t* phys_out);
extern int32_t sceKernelMapDirectMemory(void** addr, size_t len, int32_t prot, int32_t flags,
                                        off_t phys, size_t align);
extern int32_t sceKernelUsleep(uint32_t us);
extern int32_t sceKernelOpen(const char* path, int32_t flags, uint16_t mode);
extern size_t sceKernelWrite(int32_t fd, const void* buf, size_t n);
extern int32_t sceKernelClose(int32_t fd);

extern int32_t sceGnmSubmitCommandBuffers(uint32_t count, void* dcbaddrs[], uint32_t* dcbbytesizes,
                                          void* ccbaddrs[], uint32_t* ccbbytesizes);
extern int32_t sceGnmSubmitDone(void);

/* verified constants */
#define PROT_CPU_RW 0x03
#define PROT_GPU_RW 0x30   /* GPU_READ 0x10 | GPU_WRITE 0x20 */
#define MEM_TYPE_FLEX 0x03 /* memoryType arg, matches proven-working ShadCube4 */

#define O_WRONLY 0x0001
#define O_CREAT 0x0200
#define O_APPEND 0x0008

/* ---- minimal libc (nostdlib build) --------------------------------------- */
static size_t z_strlen(const char* s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
static char* z_utoa_hex(uint32_t v, char* out, int width) {
    static const char h[] = "0123456789abcdef";
    char tmp[8];
    int i = 0;
    for (i = 0; i < 8; i++) {
        tmp[i] = h[v & 0xF];
        v >>= 4;
    }
    int n = 8;
    while (n > width && tmp[n - 1] == '0')
        n--;
    for (int k = 0; k < n; k++)
        out[k] = tmp[n - 1 - k];
    return out + n;
}
static char* z_utoa_dec(uint32_t v, char* out) {
    char tmp[10];
    int i = 0;
    if (v == 0) {
        *out++ = '0';
        return out;
    }
    while (v) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--)
        *out++ = tmp[i];
    return out;
}

/* ---- logging to /data/ibchain_log.txt ------------------------------------ */
static int g_fd = -1;

static void log_open(void) {
    g_fd = sceKernelOpen("/data/ibchain_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
}

/* supports %s %u %08x %02u %x — enough for this test */
static void log_line(const char* fmt, ...) {
    char buf[512];
    char* o = buf;
    va_list ap;
    va_start(ap, fmt);
    for (const char* f = fmt; *f && o < buf + sizeof(buf) - 12; f++) {
        if (*f != '%') {
            *o++ = *f;
            continue;
        }
        f++;
        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }
        if (*f == 's') {
            const char* s = va_arg(ap, const char*);
            size_t n = z_strlen(s);
            for (size_t k = 0; k < n && o < buf + sizeof(buf) - 12; k++)
                *o++ = s[k];
        } else if (*f == 'u') {
            uint32_t v = va_arg(ap, uint32_t);
            char t[12];
            char* e = z_utoa_dec(v, t);
            for (char* p = t; p < e; p++)
                *o++ = *p;
        } else if (*f == 'x') {
            uint32_t v = va_arg(ap, uint32_t);
            char t[10];
            char* e = z_utoa_hex(v, t, width ? width : 1);
            for (char* p = t; p < e; p++)
                *o++ = *p;
        } else {
            *o++ = *f;
        }
    }
    va_end(ap);
    *o++ = '\n';
    if (g_fd >= 0)
        sceKernelWrite(g_fd, buf, (size_t)(o - buf));
}

/* flush a marker line and force it to disk before a potentially-hanging test */
static void log_progress(const char* stage, int idx) {
    log_line("PROGRESS %s test %u", stage, (uint32_t)idx);
    if (g_fd >= 0) {
        sceKernelClose(g_fd);
        g_fd = sceKernelOpen("/data/ibchain_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
    }
}

/* ---- GPU memory allocation ----------------------------------------------- */
/* One direct-memory block, carved into sub-allocations. A single Allocate+Map
 * pair instead of 17 — fewer failure modes, one contiguous GPU-visible region. */
static uint8_t* g_pool = NULL;
static uint64_t g_pool_off = 0;
static uint64_t g_pool_size = 0;

static int gpu_pool_init(uint64_t total) {
    const uint64_t page = 0x4000; /* 16 KiB PS4 direct-memory page/alignment unit */
    total = (total + (page - 1)) & ~(page - 1);
    off_t phys = 0;
    /* Args match the proven-working ShadCube4 allocator: search the full 24 GiB
     * direct-memory range and request memory type 0x03 (write-combining). */
    int32_t r = sceKernelAllocateDirectMemory(0, 0x600000000ull, total, page, MEM_TYPE_FLEX, &phys);
    if (r != 0) {
        log_line("  pool: AllocateDirectMemory failed rc=0x%x size=0x%x", (uint32_t)r,
                 (uint32_t)total);
        return 0;
    }
    void* addr = NULL;
    r = sceKernelMapDirectMemory(&addr, total, PROT_CPU_RW | PROT_GPU_RW, 0, phys, page);
    if (r != 0) {
        log_line("  pool: MapDirectMemory failed rc=0x%x phys_lo=0x%x phys_hi=0x%x", (uint32_t)r,
                 (uint32_t)(uint64_t)phys, (uint32_t)((uint64_t)phys >> 32));
        return 0;
    }
    g_pool = (uint8_t*)addr;
    g_pool_off = 0;
    g_pool_size = total;
    log_line("  pool: mapped 0x%x bytes at %x (phys_lo 0x%x)", (uint32_t)total,
             (uint32_t)(uintptr_t)addr, (uint32_t)(uint64_t)phys);
    return 1;
}

static void* gpu_alloc(uint64_t size, uint64_t align) {
    if (align < 4)
        align = 4;
    g_pool_off = (g_pool_off + (align - 1)) & ~(align - 1);
    if (!g_pool || g_pool_off + size > g_pool_size)
        return NULL;
    void* p = g_pool + g_pool_off;
    g_pool_off += size;
    return p;
}

/* ---- PM4 ----------------------------------------------------------------- */
#define PM4_TYPE3(opcode, total_dw)                                                                \
    (0xC0000000u | ((((total_dw) - 2u) & 0x3FFFu) << 16) | (((opcode) & 0xFFu) << 8))
#define IT_NOP 0x10
#define IT_WRITE_DATA 0x37
#define IT_INDIRECT_BUFFER 0x3F
#define IT_EVENT_WRITE_EOP 0x47

static uint32_t* pm4_write_dword(uint32_t* p, volatile uint32_t* dst, uint32_t val) {
    const uint64_t a = (uint64_t)(uintptr_t)dst;
    *p++ = PM4_TYPE3(IT_WRITE_DATA, 5);
    *p++ = (5u << 8) | (1u << 20); /* dst_sel=5 (memory), wr_confirm=1 */
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32);
    *p++ = val;
    return p;
}
static uint32_t* pm4_indirect_buffer(uint32_t* p, const uint32_t* target, uint32_t size_dw,
                                     uint32_t chain, uint32_t valid, uint32_t vmid) {
    const uint64_t a = (uint64_t)(uintptr_t)target;
    *p++ = PM4_TYPE3(IT_INDIRECT_BUFFER, 4);
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32) & 0xFFFFu;
    *p++ =
        (size_dw & 0xFFFFFu) | ((chain & 1u) << 20) | ((valid & 1u) << 23) | ((vmid & 0xFu) << 24);
    return p;
}

/* EVENT_WRITE_EOP: 6 dwords. Proven encoding from ShadCube4. */
static uint32_t* pm4_eop(uint32_t* p, volatile uint32_t* fence_addr, uint32_t fence_value) {
    const uint64_t a = (uint64_t)(uintptr_t)fence_addr;
    *p++ = PM4_TYPE3(IT_EVENT_WRITE_EOP, 6);
    *p++ = 0x0504u;
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32) | 0x20000000u;
    *p++ = fence_value;
    *p++ = 0;
    return p;
}

/* ---- probe ----------------------------------------------------------------
 * ONE question, measured on hardware: does a packet placed AFTER a chain=1 IB
 * hang the CP, or does the CP jump (skipping it) and complete?
 *
 * Each of the 3 sub-tests is run as its OWN submission, with a PROGRESS marker
 * flushed to disk before it. If a sub-test hangs, the last PROGRESS line names
 * it. Result slots are read after each completes.
 */
#define FENCE_DONE 0x00C0FFEE

enum {
    R_FENCE = 0,
    R_A_MARK = 1,
    R_B_MARK = 2,
    R_A_POISON = 3, /* packet placed AFTER the chain=1 IB in A */
    R_SLOTS = 8
};

static volatile uint32_t* g_res;
static uint32_t* g_A;
static uint32_t* g_B;

static int submit_and_wait(uint32_t* dcb, uint32_t dcb_dw, volatile uint32_t* fence, uint32_t want) {
    void* a[1] = {dcb};
    uint32_t s[1] = {dcb_dw * 4};
    sceGnmSubmitCommandBuffers(1, a, s, NULL, NULL);
    sceGnmSubmitDone();
    for (int w = 0; w < 200000; w++) { /* ~2 s */
        if (*fence == want)
            return 0;
        sceKernelUsleep(10);
    }
    return 1;
}

static void clear_slots(void) {
    for (int i = 0; i < R_SLOTS; i++)
        g_res[i] = 0;
}

/* Case 1 (control): chain=1 IB is the LAST packet in A. B EOPs.
 * This is the proven-good shape. Expect: complete, A+B markers set. */
static void probe1_chain_last(void) {
    log_line("PROBE 1: chain=1 IB is last packet in A (control, should pass)");
    clear_slots();
    uint32_t* p = g_B;
    p = pm4_write_dword(p, &g_res[R_B_MARK], 0xBBBBBBBB);
    p = pm4_eop(p, &g_res[R_FENCE], FENCE_DONE);
    uint32_t b_dw = (uint32_t)(p - g_B);

    p = g_A;
    p = pm4_write_dword(p, &g_res[R_A_MARK], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, g_B, b_dw, /*chain=*/1, /*valid=*/1, /*vmid=*/0);
    uint32_t a_dw = (uint32_t)(p - g_A);

    int to = submit_and_wait(g_A, a_dw, &g_res[R_FENCE], FENCE_DONE);
    log_line("  A=%08x B=%08x fence=%08x %s", g_res[R_A_MARK], g_res[R_B_MARK], g_res[R_FENCE],
             to ? "TIMEOUT" : "complete");
}

/* Case 2 (the question): a WRITE_DATA is placed AFTER the chain=1 IB in A.
 * B EOPs. If chain=1 is a clean jump, A_POISON is skipped and it completes.
 * If a trailing packet hangs the CP, this times out. */
static void probe2_packet_after_chain(void) {
    log_line("PROBE 2: WRITE_DATA placed AFTER the chain=1 IB in A (the question)");
    clear_slots();
    uint32_t* p = g_B;
    p = pm4_write_dword(p, &g_res[R_B_MARK], 0xBBBBBBBB);
    p = pm4_eop(p, &g_res[R_FENCE], FENCE_DONE);
    uint32_t b_dw = (uint32_t)(p - g_B);

    p = g_A;
    p = pm4_write_dword(p, &g_res[R_A_MARK], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, g_B, b_dw, /*chain=*/1, /*valid=*/1, /*vmid=*/0);
    p = pm4_write_dword(p, &g_res[R_A_POISON], 0xDEAD0001); /* AFTER the chain IB */
    uint32_t a_dw = (uint32_t)(p - g_A);

    int to = submit_and_wait(g_A, a_dw, &g_res[R_FENCE], FENCE_DONE);
    log_line("  A=%08x B=%08x A_POISON=%08x fence=%08x %s", g_res[R_A_MARK], g_res[R_B_MARK],
             g_res[R_A_POISON], g_res[R_FENCE], to ? "TIMEOUT(hung)" : "complete");
    if (!to) {
        if (g_res[R_A_POISON] == 0)
            log_line("  => trailing packet SKIPPED (chain=1 is a clean jump). poison test viable.");
        else
            log_line("  => trailing packet EXECUTED (CP returned). unexpected on hardware.");
    } else {
        log_line("  => a packet after a chain=1 IB HANGS the CP. poison test not viable on HW.");
    }
}

/* Case 3: chain=0 (IB2 call) with a packet after it in A. B ends (no EOP);
 * A provides the EOP after the call returns. This is the NORMAL call shape and
 * should always complete with A_POISON written (the call returns). Confirms the
 * probe harness itself and that chain=0 returns. */
static void probe3_chain0_returns(void) {
    log_line("PROBE 3: chain=0 IB2 call, packet after it, A EOPs (call must return)");
    clear_slots();
    uint32_t* p = g_B;
    p = pm4_write_dword(p, &g_res[R_B_MARK], 0xBBBBBBBB);
    uint32_t b_dw = (uint32_t)(p - g_B); /* B has NO terminator; relies on IB2 return */

    p = g_A;
    p = pm4_write_dword(p, &g_res[R_A_MARK], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, g_B, b_dw, /*chain=*/0, /*valid=*/1, /*vmid=*/0);
    p = pm4_write_dword(p, &g_res[R_A_POISON], 0xDEAD0003); /* after the call, should run */
    p = pm4_eop(p, &g_res[R_FENCE], FENCE_DONE);
    uint32_t a_dw = (uint32_t)(p - g_A);

    int to = submit_and_wait(g_A, a_dw, &g_res[R_FENCE], FENCE_DONE);
    log_line("  A=%08x B=%08x A_POISON=%08x fence=%08x %s", g_res[R_A_MARK], g_res[R_B_MARK],
             g_res[R_A_POISON], g_res[R_FENCE], to ? "TIMEOUT(hung)" : "complete");
}

static void flush_progress(int idx) {
    log_line("PROGRESS about to run probe %u", (uint32_t)idx);
    if (g_fd >= 0) {
        sceKernelClose(g_fd);
        g_fd = sceKernelOpen("/data/ibchain_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
    }
}

int main(void) {
    log_open();
    log_line("=== IB chain PROBE: does a packet after a chain=1 IB hang the CP? ===");

    if (!gpu_pool_init(64 * 1024)) {
        log_line("FATAL: gpu pool init failed");
        if (g_fd >= 0)
            sceKernelClose(g_fd);
        return 1;
    }
    g_res = (volatile uint32_t*)gpu_alloc(R_SLOTS * 4, 256);
    g_A = (uint32_t*)gpu_alloc(64 * 4, 256);
    g_B = (uint32_t*)gpu_alloc(64 * 4, 256);
    if (!g_res || !g_A || !g_B) {
        log_line("FATAL: gpu_alloc failed");
        if (g_fd >= 0)
            sceKernelClose(g_fd);
        return 1;
    }

    flush_progress(1);
    probe1_chain_last();
    flush_progress(2);
    probe2_packet_after_chain();
    flush_progress(3);
    probe3_chain0_returns();

    log_line("=== probe done ===");
    if (g_fd >= 0)
        sceKernelClose(g_fd);
    return 0;
}
