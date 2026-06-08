// ============================================================================
// PS4 GPU/CPU Race Condition Test Suite — FULL EDITION
// 30 tests covering every RAW, WAR, WAW hazard found in shadps4
// Works on real PS4 hardware (all pass) and shadps4 (exposes bugs)
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ps4_types.h"

#include "pm4.h"
#include "log.h"

// ============================================================================
// GNM / System forward declarations
// ============================================================================
extern "C" {
    int  sceGnmSubmitCommandBuffers(uint32_t count,
                                     const uint32_t* dcb_addrs[],
                                     uint32_t* dcb_sizes,
                                     const uint32_t* ccb_addrs[],
                                     uint32_t* ccb_sizes);
    int  sceGnmSubmitDone();
    int  sceGnmMapComputeQueue(uint32_t pipe_id, uint32_t queue_id,
                                uintptr_t ring_base_addr, uint32_t ring_size_dw,
                                uint32_t* read_ptr_addr);
    void sceGnmDingDong(uint32_t gnm_vqid, uint32_t next_offs_dw);
    int  sceGnmUnmapComputeQueue(uint32_t gnm_vqid);
    void sceSystemServiceLoadExec(const char* path, const char* args[]);
}

// ============================================================================
// GPU-visible memory allocator
// ============================================================================
// Pre-allocated GPU memory pool (avoids leak across runs)
#define GPU_POOL_PAGES 100
static uint8_t* g_gpu_pool = nullptr;
static int g_gpu_pool_next = 0;
static const size_t GPU_PAGE = 0x10000; // 64KB

static bool gpu_pool_init() {
    if (g_gpu_pool) return true;
    off_t phys = 0;
    size_t total = GPU_POOL_PAGES * GPU_PAGE;
    int ret = sceKernelAllocateDirectMemory(0, 0x600000000ULL, total, GPU_PAGE, 3, &phys);
    if (ret != 0) return false;
    void* ptr = nullptr;
    ret = sceKernelMapDirectMemory(&ptr, total, 0x33, 0, phys, GPU_PAGE);
    if (ret != 0) return false;
    g_gpu_pool = (uint8_t*)ptr;
    memset(g_gpu_pool, 0, total);
    return true;
}

static void gpu_pool_reset() {
    g_gpu_pool_next = 0;
    // Zero the pool for clean state
    if (g_gpu_pool) memset(g_gpu_pool, 0, GPU_POOL_PAGES * GPU_PAGE);
}

static void* gpu_alloc(size_t size, size_t align = 0x10000) {
    (void)align;
    int pages = (int)((size + GPU_PAGE - 1) / GPU_PAGE);
    if (g_gpu_pool_next + pages > GPU_POOL_PAGES) return nullptr;
    void* ptr = g_gpu_pool + (size_t)g_gpu_pool_next * GPU_PAGE;
    g_gpu_pool_next += pages;
    return ptr;
}

// ============================================================================
// Test infrastructure
// ============================================================================
static int tests_run = 0, tests_passed = 0, tests_failed = 0, tests_skipped = 0;

#define TEST_BEGIN(name) \
    do { \
        tests_run++; \
        logf("\n[TEST %d] %s\n", tests_run, name); \
        log_flush();

#define TEST_CHECK(cond, msg) \
        if (!(cond)) { \
            logf("  [FAIL] %s\n", msg); \
            tests_failed++; log_flush(); \
            break; \
        }

#define TEST_PASS() \
        logf("  [PASS]\n"); \
        tests_passed++; log_flush(); \
    } while(0)

static bool wait_fence(volatile uint64_t* fence, uint32_t timeout_us = 2000000) {
    while (*fence == 0 && timeout_us > 0) { sceKernelUsleep(10); timeout_us -= 10; }
    return *fence != 0;
}

static void submit_and_wait(uint32_t* dcb, uint32_t dcb_size_bytes,
                             uint32_t* ccb = nullptr, uint32_t ccb_size_bytes = 0) {
    const uint32_t* dp[1] = { dcb };
    uint32_t ds[1] = { dcb_size_bytes };
    const uint32_t* cp[1] = { ccb };
    uint32_t cs[1] = { ccb_size_bytes };
    sceGnmSubmitCommandBuffers(1, dp, ds, ccb ? cp : nullptr, ccb ? cs : nullptr);
    sceGnmSubmitDone();
    sceKernelUsleep(5000);
}

struct ComputeQueue {
    uint32_t* ring;
    uint32_t* read_ptr;
    int vqid;
    uint32_t write_off;

    bool init(uint32_t pipe = 0, uint32_t queue = 0) {
        // Allocate ring+read_ptr outside the pool (must survive resets)
        off_t phys1 = 0, phys2 = 0;
        sceKernelAllocateDirectMemory(0, 0x600000000ULL, 0x10000, 0x10000, 3, &phys1);
        sceKernelMapDirectMemory((void**)&ring, 0x10000, 0x33, 0, phys1, 0x10000);
        sceKernelAllocateDirectMemory(0, 0x600000000ULL, 0x10000, 0x10000, 3, &phys2);
        sceKernelMapDirectMemory((void**)&read_ptr, 0x10000, 0x33, 0, phys2, 0x10000);
        if (!ring || !read_ptr) return false;
        *read_ptr = 0; write_off = 0;
        vqid = sceGnmMapComputeQueue(pipe, queue, (uintptr_t)ring, 0x4000, read_ptr);
        return vqid > 0;
    }
    CmdBuffer begin() {
        CmdBuffer cb;
        cb.buf = ring + write_off;
        cb.offset = 0;
        cb.capacity = 0x4000 - write_off;
        return cb;
    }
    void submit(CmdBuffer& cb) {
        write_off += cb.sizeDwords();
        sceGnmDingDong((uint32_t)vqid, write_off);
    }
    void destroy() { if (vqid > 0) sceGnmUnmapComputeQueue((uint32_t)vqid); }
};

static ComputeQueue g_cq;
static ComputeQueue g_cq1;

// ============================================================================
//  CATEGORY 1: FENCE TIMING (RAW-1, RAW-2)
// ============================================================================

static void test_01_eop_fence_timing() {
    TEST_BEGIN("RAW-1: EOP fence fires before GPU DMA completes");
    volatile uint32_t* data = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* fence = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(data && fence, "alloc");
    *data = 0; *fence = 0;

    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_dma_data_fill(cb, (void*)data, 0xDEADBEEF, 4);
    pm4_event_write_eop(cb, (void*)fence, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes());
    TEST_CHECK(wait_fence(fence), "fence timeout");
    TEST_CHECK(*data == 0xDEADBEEF, "RAW-1: DMA data stale when fence signaled");
    TEST_PASS();
}

static void test_02_release_mem_timing() {
    TEST_BEGIN("RAW-2: Compute ReleaseMem fence fires before DMA completes");
    volatile uint32_t* data = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* fence = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(data && fence, "alloc");
    *data = 0; *fence = 0;
    CmdBuffer acb = g_cq.begin();
    pm4_dma_data_fill(acb, (void*)data, 0xBAADF00D, 4);
    pm4_release_mem(acb, (void*)fence, 1, 2, 0);
    pm4_nop(acb);
    g_cq.submit(acb);
    sceKernelUsleep(20000);
    TEST_CHECK(wait_fence(fence), "fence timeout");
    TEST_CHECK(*data == 0xBAADF00D, "RAW-2: Compute DMA stale when ReleaseMem arrived");
    TEST_PASS();
}

static void test_03_eop_pipeline_depth() {
    TEST_BEGIN("RAW-1b: 8 sequential EOP fences preserve ordering");
    const int N = 8;
    volatile uint64_t* fences = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint32_t* datas = (volatile uint32_t*)gpu_alloc(0x10000);
    TEST_CHECK(fences && datas, "alloc");
    memset((void*)fences, 0, N * 8); memset((void*)datas, 0, N * 4);
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    for (int i = 0; i < N; i++) {
        pm4_dma_data_fill(cb, (void*)&datas[i], (uint32_t)(i + 1), 4);
        pm4_event_write_eop(cb, (void*)&fences[i], (uint64_t)(i + 1), 2, 0);
    }
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes());
    TEST_CHECK(wait_fence(&fences[N - 1]), "last fence timeout");
    bool ok = true;
    for (int i = 0; i < N; i++) {
        if (fences[i] != (uint64_t)(i + 1) || datas[i] != (uint32_t)(i + 1)) { ok = false; break; }
    }
    TEST_CHECK(ok, "Pipeline EOP fence ordering violated");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 2: CACHE COHERENCE (WAW-5, WAW-6, RAW-8)
// ============================================================================

static void test_04_acquire_mem() {
    TEST_BEGIN("WAW-5: AcquireMem cache invalidation is no-op");
    volatile uint32_t* src = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* dst = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f1 = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* f2 = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(src && dst && f1 && f2, "alloc");
    *src = 0; *dst = 0; *f1 = 0; *f2 = 0;

    uint32_t* d1 = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer c1; c1.init(d1, 0x4000);
    pm4_context_control(c1);
    pm4_dma_data_fill(c1, (void*)src, 0xCAFEBABE, 4);
    pm4_event_write_eop(c1, (void*)f1, 1, 2, 0);
    pm4_nop(c1);
    submit_and_wait(d1, c1.sizeBytes()); if (!wait_fence(f1)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }

    uint32_t* d2 = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer c2; c2.init(d2, 0x4000);
    pm4_context_control(c2);
    pm4_acquire_mem(c2);
    pm4_dma_data_copy(c2, (void*)dst, (void*)src, 4);
    pm4_event_write_eop(c2, (void*)f2, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(d2, c2.sizeBytes()); if (!wait_fence(f2)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*dst == 0xCAFEBABE, "WAW-5: AcquireMem no-op, copy got stale data");
    TEST_PASS();
}

static void test_05_cp_sync_width() {
    TEST_BEGIN("WAW-6: PfpSyncMe barrier too narrow");
    volatile uint32_t* a = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* b = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(a && b && f, "alloc");
    *a = 0; *b = 0; *f = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_dma_data_fill(cb, (void*)a, 0xBAADF00D, 4);
    pm4_pfp_sync_me(cb);
    pm4_dma_data_copy(cb, (void*)b, (void*)a, 4);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*b == 0xBAADF00D, "WAW-6: PfpSyncMe didn't barrier fill before copy");
    TEST_PASS();
}

static void test_06_ce_dump_const_ram() {
    TEST_BEGIN("RAW-8: CE DumpConstRam bypasses dirty tracking");
    volatile uint32_t* target = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* fence = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(target && fence, "alloc");
    *target = 0; *fence = 0;
    uint32_t* ccb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer ccb; ccb.init(ccb_mem, 0x4000);
    uint32_t val = 0x12345678;
    pm4_write_const_ram(ccb, 0, &val, 1);
    pm4_dump_const_ram(ccb, (void*)target, 0, 1);
    pm4_increment_ce_counter(ccb);
    uint32_t* dcb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer dcb; dcb.init(dcb_mem, 0x4000);
    pm4_context_control(dcb);
    pm4_wait_on_ce_counter(dcb);
    pm4_increment_de_counter(dcb);
    pm4_event_write_eop(dcb, (void*)fence, 1, 2, 0);
    pm4_nop(dcb);
    submit_and_wait(dcb_mem, dcb.sizeBytes(), ccb_mem, ccb.sizeBytes());
    if (!wait_fence(fence)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*target == 0x12345678, "RAW-8: CE dump data not visible");
    TEST_PASS();
}

static void test_07_ce_stress() {
    TEST_BEGIN("RAW-8b: CE constant heap 8x write+dump");
    const int N = 8;
    volatile uint32_t* t = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(t && f, "alloc");
    memset((void*)t, 0, N * 4); *f = 0;
    uint32_t* ccb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer ccb; ccb.init(ccb_mem, 0x4000);
    for (int i = 0; i < N; i++) {
        uint32_t v = 0xA0000000u | (uint32_t)i;
        pm4_write_const_ram(ccb, (uint32_t)(i * 4), &v, 1);
        pm4_dump_const_ram(ccb, (void*)&t[i], (uint32_t)(i * 4), 1);
    }
    pm4_increment_ce_counter(ccb);
    uint32_t* dcb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer dcb; dcb.init(dcb_mem, 0x4000);
    pm4_context_control(dcb);
    pm4_wait_on_ce_counter(dcb);
    pm4_increment_de_counter(dcb);
    pm4_event_write_eop(dcb, (void*)f, 1, 2, 0);
    pm4_nop(dcb);
    submit_and_wait(dcb_mem, dcb.sizeBytes(), ccb_mem, ccb.sizeBytes());
    if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    bool ok = true;
    for (int i = 0; i < N; i++) {
        if (t[i] != (0xA0000000u | (uint32_t)i)) { ok = false; break; }
    }
    TEST_CHECK(ok, "CE multi-dump: entries not visible");
    TEST_PASS();
}

static void test_08_acquire_mem_cross_engine() {
    TEST_BEGIN("WAW-5b: AcquireMem cross-engine (compute->GFX)");
    volatile uint32_t* buf = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* dst = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f1 = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* f2 = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(buf && dst && f1 && f2, "alloc");
    *buf = 0; *dst = 0; *f1 = 0; *f2 = 0;
    CmdBuffer acb = g_cq.begin();
    pm4_dma_data_fill(acb, (void*)buf, 0xFEEDFACE, 4);
    pm4_release_mem(acb, (void*)f1, 1, 2, 0);
    pm4_nop(acb);
    g_cq.submit(acb); sceKernelUsleep(10000); if (!wait_fence(f1)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }

    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_acquire_mem(cb);
    pm4_dma_data_copy(cb, (void*)dst, (void*)buf, 4);
    pm4_event_write_eop(cb, (void*)f2, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f2)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*dst == 0xFEEDFACE, "WAW-5b: AcquireMem didn't flush compute DMA for GFX");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 3: MEM_SEMAPHORE (WAW-1)
// ============================================================================

static void test_09_mem_semaphore_basic() {
    TEST_BEGIN("WAW-1: MemSemaphore 5x signal + 5x wait");
    volatile uint64_t* sem = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(sem && f, "alloc"); *sem = 0; *f = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    for (int i = 0; i < 5; i++) pm4_mem_semaphore_signal(cb, (void*)sem);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*sem == 5, "Signal count wrong");
    *f = 0;
    CmdBuffer c2; c2.init(dcb, 0x4000);
    pm4_context_control(c2);
    for (int i = 0; i < 5; i++) pm4_mem_semaphore_wait(c2, (void*)sem);
    pm4_event_write_eop(c2, (void*)f, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(dcb, c2.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*sem == 0, "WAW-1: Semaphore not zero after wait/decrement");
    TEST_PASS();
}

static void test_10_mem_semaphore_cross_queue() {
    TEST_BEGIN("WAW-1b: Cross-queue MemSemaphore (GFX signal, Compute wait)");
    volatile uint64_t* sem = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* gf = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* cf = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(sem && gf && cf, "alloc"); *sem = 0; *gf = 0; *cf = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer gfx; gfx.init(dcb, 0x4000);
    pm4_context_control(gfx);
    pm4_mem_semaphore_signal(gfx, (void*)sem);
    pm4_event_write_eop(gfx, (void*)gf, 1, 2, 0);
    pm4_nop(gfx);
    CmdBuffer acb = g_cq.begin();
    pm4_mem_semaphore_wait(acb, (void*)sem);
    pm4_release_mem(acb, (void*)cf, 1, 2, 0);
    pm4_nop(acb);
    const uint32_t* dp[1] = { dcb }; uint32_t ds[1] = { gfx.sizeBytes() };
    sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
    g_cq.submit(acb); sceGnmSubmitDone(); sceKernelUsleep(30000);
    TEST_CHECK(wait_fence(gf, 100000), "GFX fence timeout");
    TEST_CHECK(wait_fence(cf, 500000), "WAW-1b: Compute never unblocked by GFX semaphore");
    TEST_PASS();
}

static void test_11_mem_semaphore_stress() {
    TEST_BEGIN("WAW-1c: MemSemaphore 32x signal + 32x wait");
    volatile uint64_t* sem = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(sem && f, "alloc"); *sem = 0; *f = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    for (int i = 0; i < 32; i++) pm4_mem_semaphore_signal(cb, (void*)sem);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*sem == 32, "32x signal count wrong");
    *f = 0;
    CmdBuffer c2; c2.init(dcb, 0x4000);
    pm4_context_control(c2);
    for (int i = 0; i < 32; i++) pm4_mem_semaphore_wait(c2, (void*)sem);
    pm4_event_write_eop(c2, (void*)f, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(dcb, c2.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*sem == 0, "WAW-1c: 32x signal/wait imbalanced");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 4: CROSS-QUEUE ORDERING (WAW-2)
// ============================================================================

static void test_12_cross_queue_fence() {
    TEST_BEGIN("WAW-2: GFX+Compute fence to same label determinism");
    volatile uint64_t* fence = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(fence, "alloc");
    // Using global g_cq
    bool consistent = true; uint64_t first = 0;
    for (int t = 0; t < 10; t++) {
        *fence = 0; __asm__ volatile("" ::: "memory");
        uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
        CmdBuffer gfx; gfx.init(dcb, 0x4000);
        pm4_context_control(gfx);
        pm4_event_write_eop(gfx, (void*)fence, 0xAAAA, 2, 0);
        pm4_nop(gfx);
        CmdBuffer acb = g_cq.begin();
        pm4_release_mem(acb, (void*)fence, 0xBBBB, 2, 0);
        pm4_nop(acb);
        const uint32_t* dp[1] = { dcb }; uint32_t ds[1] = { gfx.sizeBytes() };
        sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
        g_cq.submit(acb); sceGnmSubmitDone(); sceKernelUsleep(20000);
        if (t == 0) first = *fence;
        if (*fence != first) { consistent = false; break; }
    }
    logf("  Final=0x%llX\n", (unsigned long long)*fence);
    TEST_CHECK(consistent, "WAW-2: Cross-queue fence non-deterministic");
    TEST_PASS();
}

static void test_13_two_compute_queues() {
    TEST_BEGIN("WAW-2b: Two compute queues racing on same fence");
    volatile uint64_t* fence = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(fence, "alloc");
    if (g_cq1.vqid <= 0) {
        if (!g_cq1.init(0, 1)) { logf("  [SKIP] cq1 failed\n"); tests_skipped++; }
    }
    bool consistent = true; uint64_t first = 0;
    for (int t = 0; t < 10; t++) {
        *fence = 0; __asm__ volatile("" ::: "memory");
        CmdBuffer a0 = g_cq.begin();
        pm4_release_mem(a0, (void*)fence, 0x1111, 2, 0); pm4_nop(a0);
        CmdBuffer a1 = g_cq1.begin();
        pm4_release_mem(a1, (void*)fence, 0x2222, 2, 0); pm4_nop(a1);
        g_cq.submit(a0); g_cq1.submit(a1); sceKernelUsleep(20000);
        if (t == 0) first = *fence;
        if (*fence != first) { consistent = false; break; }
    }
    logf("  Final=0x%llX\n", (unsigned long long)*fence);
    TEST_CHECK(consistent, "WAW-2b: Two compute queues non-deterministic");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 5: WAIT_REG_MEM (all comparison functions)
// ============================================================================

static void test_wait_reg_mem_func(const char* name, uint32_t write_val,
                                    uint32_t ref, uint32_t mask, uint32_t func) {
    TEST_BEGIN(name);
    volatile uint32_t* label = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(label && f, "alloc"); *label = 0; *f = 0;
    uint32_t* d1 = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer c1; c1.init(d1, 0x4000);
    pm4_context_control(c1);
    pm4_write_data_u32(c1, label, write_val);
    pm4_nop(c1);
    submit_and_wait(d1, c1.sizeBytes()); sceKernelUsleep(5000);
    uint32_t* d2 = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer c2; c2.init(d2, 0x4000);
    pm4_context_control(c2);
    pm4_wait_reg_mem(c2, (void*)label, ref, mask, func);
    pm4_event_write_eop(c2, (void*)f, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(d2, c2.sizeBytes());
    TEST_CHECK(wait_fence(f), "WaitRegMem timed out");
    TEST_PASS();
}

static void test_14_wrm_equal()     { test_wait_reg_mem_func("WaitRegMem: Equal",        42, 42, 0xFFFFFFFF, 3); }
static void test_15_wrm_gt()        { test_wait_reg_mem_func("WaitRegMem: GreaterThan",   11, 10, 0xFFFFFFFF, 6); }
static void test_16_wrm_lt()        { test_wait_reg_mem_func("WaitRegMem: LessThan",      50,100, 0xFFFFFFFF, 1); }
static void test_17_wrm_masked()    { test_wait_reg_mem_func("WaitRegMem: Masked Equal", 0xFF000042, 0x42, 0xFF, 3); }
static void test_18_wrm_gte()       { test_wait_reg_mem_func("WaitRegMem: GreaterEqual",  10, 10, 0xFFFFFFFF, 5); }
static void test_19_wrm_not_equal() { test_wait_reg_mem_func("WaitRegMem: NotEqual",       7,  8, 0xFFFFFFFF, 4); }

// ============================================================================
//  CATEGORY 6: DMA ORDERING
// ============================================================================

static void test_20_dma_sequential() {
    TEST_BEGIN("DMA: Sequential fills (last=5)");
    volatile uint32_t* t = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(t && f, "alloc"); *t = 0; *f = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    for (uint32_t i = 1; i <= 5; i++) pm4_dma_data_fill(cb, (void*)t, i, 4);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*t == 5, "DMA order violated");
    TEST_PASS();
}

static void test_21_dma_copy_chain() {
    TEST_BEGIN("DMA: Copy chain A->B->C");
    volatile uint32_t* a = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* b = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* c = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(a && b && c && f, "alloc"); *a=0;*b=0;*c=0;*f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_dma_data_fill(cb, (void*)a, 0x55AA55AA, 4);
    pm4_dma_data_copy(cb, (void*)b, (void*)a, 4);
    pm4_dma_data_copy(cb, (void*)c, (void*)b, 4);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*c == 0x55AA55AA, "DMA chain: data lost");
    TEST_PASS();
}

static void test_22_dma_large_block() {
    TEST_BEGIN("DMA: 4KB fill integrity");
    volatile uint32_t* buf = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(buf && f, "alloc"); memset((void*)buf, 0, 4096); *f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_dma_data_fill(cb, (void*)buf, 0xABCDABCD, 4096);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    bool ok = true;
    for (int i = 0; i < 1024; i++) { if (buf[i] != 0xABCDABCD) { ok=false; break; } }
    TEST_CHECK(ok, "DMA 4KB fill corrupted");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 7: WRITE_DATA & WAW-4
// ============================================================================

static void test_23_write_data_u32() {
    TEST_BEGIN("WriteData: u32 visibility after EOP");
    volatile uint32_t* d = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(d && f, "alloc"); *d=0;*f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb); pm4_write_data_u32(cb, d, 0xFACEFEED);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*d == 0xFACEFEED, "WriteData u32 fail");
    TEST_PASS();
}

static void test_24_write_data_u64() {
    TEST_BEGIN("WriteData: u64 visibility");
    volatile uint64_t* d = (volatile uint64_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(d && f, "alloc"); *d=0;*f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb); pm4_write_data_u64(cb, d, 0x0123456789ABCDEFULL);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*d == 0x0123456789ABCDEFULL, "WriteData u64 fail");
    TEST_PASS();
}

static void test_25_write_data_then_dma() {
    TEST_BEGIN("WAW-4: WriteData then DMA fill same address (DMA wins)");
    volatile uint32_t* d = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(d && f, "alloc"); *d=0;*f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_write_data_u32(cb, d, 0x11111111);
    pm4_dma_data_fill(cb, (void*)d, 0x22222222, 4);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*d == 0x22222222, "WAW-4: DMA should overwrite WriteData");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 8: COND_EXEC (RAW-9)
// ============================================================================

static void test_26_cond_exec_skip() {
    TEST_BEGIN("CondExec: skip when bool==0");
    volatile uint32_t* b = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* r = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(b && r && f, "alloc"); *b=0; *r=0; *f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_cond_exec(cb, (void*)b, 5);
    pm4_write_data_u32(cb, r, 0xDEAD);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*r == 0, "CondExec should skip WriteData when bool==0");
    TEST_PASS();
}

static void test_27_cond_exec_run() {
    TEST_BEGIN("CondExec: execute when bool!=0");
    volatile uint32_t* b = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint32_t* r = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(b && r && f, "alloc"); *b=1; *r=0; *f=0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_cond_exec(cb, (void*)b, 5);
    pm4_write_data_u32(cb, r, 0xBEEF);
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*r == 0xBEEF, "CondExec should execute WriteData when bool!=0");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 9: INDIRECT BUFFER
// ============================================================================

static void test_28_indirect_buffer() {
    TEST_BEGIN("IndirectBuffer: secondary cmd buffer execution");
    volatile uint32_t* r = (volatile uint32_t*)gpu_alloc(0x10000);
    volatile uint64_t* f = (volatile uint64_t*)gpu_alloc(0x10000);
    TEST_CHECK(r && f, "alloc"); *r=0; *f=0;
    uint32_t* ib = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer ib_cb; ib_cb.init(ib, 0x4000);
    pm4_write_data_u32(ib_cb, r, 0x1B1B1B1B);
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_indirect_buffer(cb, ib, ib_cb.sizeDwords());
    pm4_event_write_eop(cb, (void*)f, 1, 2, 0); pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout\n"); tests_failed++; log_flush(); break; }
    TEST_CHECK(*r == 0x1B1B1B1B, "IB: WriteData not executed");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 10: SUBMIT STRESS (WAR-1)
// ============================================================================

static void test_29_rapid_submit() {
    TEST_BEGIN("WAR-1: 50 rapid submit/done cycles");
    const int N = 50;
    volatile uint32_t* fences = (volatile uint32_t*)gpu_alloc(0x10000);
    TEST_CHECK(fences, "alloc"); memset((void*)fences, 0, N * 4);
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    for (int i = 0; i < N; i++) {
        CmdBuffer cb; cb.init(dcb, 0x4000);
        pm4_context_control(cb);
        pm4_write_data_u32(cb, &fences[i], (uint32_t)(i + 1));
        pm4_nop(cb);
        const uint32_t* dp[1] = { dcb }; uint32_t ds[1] = { cb.sizeBytes() };
        sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
        sceGnmSubmitDone();
    }
    sceKernelUsleep(100000);
    int bad = 0;
    for (int i = 0; i < N; i++)
        if (fences[i] != (uint32_t)(i + 1)) bad++;
    TEST_CHECK(bad == 0, "WAR-1: Rapid submits corrupted");
    TEST_PASS();
}

static void test_30_rapid_submit_same_addr() {
    TEST_BEGIN("WAR-1b: 100 submits to same address (last=100)");
    volatile uint32_t* val = (volatile uint32_t*)gpu_alloc(0x10000);
    TEST_CHECK(val, "alloc"); *val = 0;
    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    for (int i = 1; i <= 100; i++) {
        CmdBuffer cb; cb.init(dcb, 0x4000);
        pm4_context_control(cb);
        pm4_write_data_u32(cb, val, (uint32_t)i);
        pm4_nop(cb);
        const uint32_t* dp[1] = { dcb }; uint32_t ds[1] = { cb.sizeBytes() };
        sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
        sceGnmSubmitDone();
    }
    sceKernelUsleep(200000);
    TEST_CHECK(*val == 100, "WAR-1b: Last submit should win");
    TEST_PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    log_init();

    // Init GPU memory pool (one big allocation, reused across all runs)
    if (!gpu_pool_init()) {
        logf("FATAL: GPU pool init failed\n");
        log_close();
        return 1;
    }

    // Init persistent compute queues (shadps4 deadlocks on unmap+remap)
    if (!g_cq.init(0, 0)) {
        logf("FATAL: compute queue 0 init failed\n");
        log_close();
        return 1;
    }



    logf("========================================================\n");
    logf(" PS4 GPU/CPU Race Condition Test Suite - FULL EDITION\n");
    logf(" 30 Tests | Target: Real PS4 + shadps4\n");
    logf("========================================================\n");

    const int RUNS = 3;
    int total_pass = 0, total_fail = 0;

    for (int _run = 0; _run < RUNS; _run++) {
        tests_run = 0; tests_passed = 0; tests_failed = 0; tests_skipped = 0;
        logf("\n===== RUN %d/%d =====\n", _run+1, RUNS);
        g_cq.write_off = 0; // reset ring position
        sceKernelUsleep(500000); // 500ms drain before reset
        gpu_pool_reset();

        test_01_eop_fence_timing();
        test_02_release_mem_timing();
        test_03_eop_pipeline_depth();
        test_04_acquire_mem();
        test_05_cp_sync_width();
        test_06_ce_dump_const_ram();
        test_07_ce_stress();
        test_08_acquire_mem_cross_engine();
        test_09_mem_semaphore_basic();
        test_10_mem_semaphore_cross_queue();
        test_11_mem_semaphore_stress();
        test_12_cross_queue_fence();
        test_13_two_compute_queues();
        test_14_wrm_equal();
        test_15_wrm_gt();
        test_16_wrm_lt();
        test_17_wrm_masked();
        test_18_wrm_gte();
        test_19_wrm_not_equal();
        test_20_dma_sequential();
        test_21_dma_copy_chain();
        test_22_dma_large_block();
        test_23_write_data_u32();
        test_24_write_data_u64();
        test_25_write_data_then_dma();
        test_26_cond_exec_skip();
        test_27_cond_exec_run();
        test_28_indirect_buffer();
        test_29_rapid_submit();
        test_30_rapid_submit_same_addr();

        total_pass += tests_passed;
        total_fail += tests_failed;
    }


    logf("\n========================================================\n");
    logf(" Results: %d passed, %d failed / %d total (across %d runs)\n",
           total_pass, total_fail, total_pass + total_fail, RUNS);
    logf("========================================================\n");
    if (total_fail == 0) logf(" ALL %d TESTS PASSED\n", total_pass);
    else logf(" %d HAZARDS DETECTED (out of %d)\n", total_fail, total_pass + total_fail);
    logf("========================================================\n");

    g_cq.destroy();
    if (g_cq1.vqid > 0) g_cq1.destroy();
    log_close();
    sceKernelUsleep(5000000);
    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
