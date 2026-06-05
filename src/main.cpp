// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// vdec2_capture: PS4 homebrew that calls sceVideodec2QueryDecoderMemoryInfo
// across a comprehensive grid of decoder configs and writes results to
// /data/vdec2_capture.json.
//
// Phases (each entry is tagged with "probe"):
//   "grid"   - full profile x level x (W,H) x dpb x pipe x opt sweep
//   "wprobe" - fine width sweep at fixed height, step 1px, to make the luma
//              pitch alignment quantum directly observable as a step function
//   "hprobe" - fine height sweep at fixed width, step 1px, for the height quantum
//   "p10"    - 10-bit / high-profile probe (110/122/244) to see if supported
//              and whether the pitch changes for >8-bit
//
// User feedback while running:
//   - PS4 toast at start, at every 10% milestone (with ETA), and at end
//   - Live status file at /data/vdec2_capture.status updated every 200
//     iterations with phase / processed / total / percent / eta / rate
//     (FTP-pollable in real time)
//
// This source is intentionally self-contained - it does NOT include any
// <orbis/...> headers. The OpenOrbis SDK's Videodec2.h only ships a stub
// declaration `void sceVideodec2QueryDecoderMemoryInfo();`, so the struct
// layouts and function prototype are declared inline. Layouts RE'd from
// libSceVideodec2.sprx (FW 9.00) and verified against shadPS4 and against the
// resulting capture (frame-buffer size fits AlignUp(w,256)*AlignUp(h,16)*3/2
// + const exactly, which cross-validates the maxFrameBufferSize field offset).

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- PS4 system symbols (link against libkernel + libSceVideodec2) --

extern "C" int sceKernelDebugOutText(int channel, const char* text);
extern "C" uint64_t sceKernelGetProcessTime(void); // microseconds since start

// ---------- libSceVideodec2 ABI (RE'd, not in OpenOrbis headers) ----------
//
// Both structs are exactly 0x48 bytes. The PS4 firmware validates this
// strictly at libSceVideodec2 0xba1/0xba6 (`cmp [r12], 0x48; jne err`).

struct OrbisVideodec2DecoderConfigInfo {
    uint64_t thisSize;                // 0x00 - must be 0x48
    uint32_t resourceType;            // 0x08
    uint32_t codecType;               // 0x0c - 1 = AVC
    uint32_t profile;                 // 0x10
    uint32_t maxLevel;                // 0x14
    int32_t maxFrameWidth;            // 0x18 - -1 for auto-dim
    int32_t maxFrameHeight;           // 0x1c - -1 for auto-dim
    int32_t maxDpbFrameCount;         // 0x20
    uint32_t decodePipelineDepth;     // 0x24
    void* computeQueue;               // 0x28
    uint64_t cpuAffinityMask;         // 0x30
    int32_t cpuThreadPriority;        // 0x38
    uint8_t optimizeProgressiveVideo; // 0x3c
    uint8_t checkMemoryType;          // 0x3d
    uint8_t reserved0;                // 0x3e
    uint8_t reserved1;                // 0x3f
    void* extraConfigInfo;            // 0x40
};
static_assert(sizeof(OrbisVideodec2DecoderConfigInfo) == 0x48,
              "DecoderConfigInfo must be 0x48 bytes");

struct OrbisVideodec2DecoderMemoryInfo {
    uint64_t thisSize;             // 0x00 - must be 0x48
    uint64_t cpuMemorySize;        // 0x08
    void* cpuMemory;               // 0x10
    uint64_t gpuMemorySize;        // 0x18
    void* gpuMemory;               // 0x20
    uint64_t cpuGpuMemorySize;     // 0x28
    void* cpuGpuMemory;            // 0x30
    uint64_t maxFrameBufferSize;   // 0x38
    uint32_t frameBufferAlignment; // 0x40
    uint32_t reserved0;            // 0x44
};
static_assert(sizeof(OrbisVideodec2DecoderMemoryInfo) == 0x48,
              "DecoderMemoryInfo must be 0x48 bytes");

extern "C" int32_t
sceVideodec2QueryDecoderMemoryInfo(const OrbisVideodec2DecoderConfigInfo* decoderConfigInfo,
                                   OrbisVideodec2DecoderMemoryInfo* decoderMemoryInfo);

// ---------- PS4 toast notification (libkernel) ----------------------------
//
// Layout RE'd by OSM-Made/PS4-Notify. Total size must be exactly 0xC30 bytes
// (3120) - the kernel rejects anything else. Verified by static_assert.
// Channel 0 = ToastPopup (banner top-right), channel 1 = NotifyDatabase.

#pragma pack(push, 1)
struct OrbisNotificationRequest {
    uint32_t type;             // 0x00 - 0 = Message
    uint32_t reqId;            // 0x04
    uint32_t priority;         // 0x08 - 0 = Default
    uint32_t msgId;            // 0x0c
    uint32_t targetId;         // 0x10 - -1 = all users
    uint32_t userId;           // 0x14
    uint32_t deviceId;         // 0x18
    uint32_t addressingUserId; // 0x1c
    uint32_t appId;            // 0x20
    uint32_t errorNumber;      // 0x24
    uint32_t attribute;        // 0x28 - 0 = no special attrs
    uint8_t hasIcon;           // 0x2c
    char message[0x400];       // 0x2d (1024 bytes)
    char iconImageUri[0x800];  // 0x42d (2048 bytes)
    char padding[3];           // 0xc2d - pad to 0xc30
};
#pragma pack(pop)
static_assert(sizeof(OrbisNotificationRequest) == 0xC30,
              "OrbisNotificationRequest must be exactly 0xC30 bytes");

extern "C" int32_t sceKernelSendNotificationRequest(int32_t api, void* request, size_t size,
                                                    int32_t blocking);

static void ps4_notify(const char* fmt, ...) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;      // Message
    req.targetId = -1; // all users
    req.hasIcon = 1;
    // tex_default_icon_notification = round 'i' speech bubble
    strncpy(req.iconImageUri, "cxml://psnotification/tex_default_icon_notification",
            sizeof(req.iconImageUri) - 1);

    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message) - 1, fmt, args);
    va_end(args);

    sceKernelSendNotificationRequest(0 /* ToastPopup */, &req, sizeof(req), 0);
}

// ---------- Capture grid --------------------------------------------------

// Invariant config inputs (single source of truth for both the call and the
// emitted JSON, so the JSON can never drift from what was actually passed).
static const int32_t CPU_AFFINITY_MASK = 0x3F;
static const int32_t CPU_THREAD_PRIORITY = 700;

struct ConfigCombo {
    int32_t resourceType;
    int32_t codecType;
    int32_t profile;
    int32_t maxLevel;
    int32_t maxFrameWidth;
    int32_t maxFrameHeight;
    int32_t maxDpbFrameCount;
    int32_t decodePipelineDepth;
    int32_t optimizeProgressiveVideo;
};

static const int32_t AVC_PROFILES[] = {66, 77, 100};
static const int32_t AVC_LEVELS[] = {10, 11, 12, 13, 20, 21, 22, 30,
                                     31, 32, 40, 41, 42, 50, 51, 52};
static const int32_t COMMON_DIMS[][2] = {
    {-1, -1}, // auto-dim: kernel uses 4096x4096 for L=52 else 4096x2176
    {160, 128},  {176, 144},   {352, 288},   {640, 480},   {720, 480},   {720, 576},
    {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {4096, 2160}, {4096, 2176},
};

// Step-function probes: a single high level that admits every probe dimension.
static const int32_t PROBE_LEVEL = 51;
// Width probe: step 1px at a fixed 16-aligned height, spanning the 256 and 512
// pitch boundaries so the alignment quantum shows up as a step in the size.
static const int32_t WPROBE_MIN = 16;
static const int32_t WPROBE_MAX = 560;
static const int32_t WPROBE_HEIGHT = 272;
// Height probe: step 1px at a fixed 256-aligned width.
static const int32_t HPROBE_MIN = 16;
static const int32_t HPROBE_MAX = 272;
static const int32_t HPROBE_WIDTH = 512;
// High-profile / 10-bit probe.
static const int32_t P10_PROFILES[] = {110, 122, 244};
static const int32_t P10_DIMS[][2] = {{1920, 1080}, {1280, 720}};

static FILE* g_out = nullptr;
static int g_count = 0;
static int g_ok = 0;
static int g_err = 0;
static int g_total = 0;
static int g_next_milestone = 10; // next percent at which to toast
static uint64_t g_start_us = 0;

static int compute_total() {
    const int n_dims = (int)(sizeof(COMMON_DIMS) / sizeof(COMMON_DIMS[0]));
    const int n_levels = (int)(sizeof(AVC_LEVELS) / sizeof(AVC_LEVELS[0]));
    const int n_profiles = (int)(sizeof(AVC_PROFILES) / sizeof(AVC_PROFILES[0]));
    const int n_p10_prof = (int)(sizeof(P10_PROFILES) / sizeof(P10_PROFILES[0]));
    const int n_p10_dims = (int)(sizeof(P10_DIMS) / sizeof(P10_DIMS[0]));
    int grid = n_profiles * n_levels * n_dims * 4 /*dpb*/ * 4 /*pipe*/ * 2 /*opt*/;
    int wprobe = WPROBE_MAX - WPROBE_MIN + 1;
    int hprobe = HPROBE_MAX - HPROBE_MIN + 1;
    int p10 = n_p10_prof * n_p10_dims;
    return grid + wprobe + hprobe + p10;
}

static void update_status_file(const char* phase) {
    FILE* sf = fopen("/data/vdec2_capture.status", "w");
    if (!sf) {
        return;
    }
    int pct = g_total > 0 ? (int)((int64_t)g_count * 100 / g_total) : 0;
    uint64_t elapsed_us = sceKernelGetProcessTime() - g_start_us;
    int rate = elapsed_us > 0 ? (int)((int64_t)g_count * 1000000 / elapsed_us) : 0;
    int eta_s = 0;
    if (g_count > 0 && g_count < g_total) {
        uint64_t eta_us = (uint64_t)(g_total - g_count) * elapsed_us / (uint64_t)g_count;
        eta_s = (int)(eta_us / 1000000);
    }
    fprintf(sf,
            "phase: %s\n"
            "processed: %d\n"
            "total: %d\n"
            "percent: %d\n"
            "ok: %d\n"
            "errors: %d\n"
            "rate_per_s: %d\n"
            "eta_s: %d\n"
            "output: /data/vdec2_capture.json\n",
            phase, g_count, g_total, pct, g_ok, g_err, rate, eta_s);
    fclose(sf);
}

static void emit_entry(const ConfigCombo& c, const OrbisVideodec2DecoderMemoryInfo& m,
                       int32_t status, const char* probe) {
    if (g_count > 0) {
        fputs(",\n", g_out);
    }
    fputs("  {", g_out);
    if (status != 0) {
        fprintf(g_out, "\"status\":\"error 0x%08x\",", (unsigned)status);
    }
    fprintf(g_out, "\"probe\":\"%s\",", probe);
    fprintf(g_out,
            "\"decoderConfigInfo\":{"
            "\"resourceType\":\"%d\",\"codecType\":\"%d\","
            "\"profile\":\"%d\",\"maxLevel\":\"%d\","
            "\"maxFrameWidth\":\"%d\",\"maxFrameHeight\":\"%d\","
            "\"maxDpbFrameCount\":\"%d\",\"decodePipelineDepth\":\"%d\","
            "\"cpuAffinityMask\":\"%d\",\"cpuThreadPriority\":\"%d\","
            "\"optimizeProgressiveVideo\":\"%d\"}",
            c.resourceType, c.codecType, c.profile, c.maxLevel, c.maxFrameWidth, c.maxFrameHeight,
            c.maxDpbFrameCount, c.decodePipelineDepth, CPU_AFFINITY_MASK, CPU_THREAD_PRIORITY,
            c.optimizeProgressiveVideo);
    if (status == 0) {
        fprintf(g_out,
                ",\"decoderMemoryInfo\":{"
                "\"cpuMemorySize\":\"%llu\","
                "\"gpuMemorySize\":\"%llu\","
                "\"cpuGpuMemorySize\":\"%llu\","
                "\"maxFrameBufferSize\":\"%llu\","
                "\"frameBufferAlignment\":\"%u\"}",
                (unsigned long long)m.cpuMemorySize, (unsigned long long)m.gpuMemorySize,
                (unsigned long long)m.cpuGpuMemorySize, (unsigned long long)m.maxFrameBufferSize,
                (unsigned)m.frameBufferAlignment);
    }
    fputs("}", g_out);
}

static void maybe_progress(const char* phase) {
    if ((g_count % 200) == 0) {
        update_status_file(phase);
    }
    int pct = g_total > 0 ? (int)((int64_t)g_count * 100 / g_total) : 0;
    if (pct >= g_next_milestone && g_count < g_total) {
        uint64_t elapsed_us = sceKernelGetProcessTime() - g_start_us;
        int eta_s = 0;
        if (g_count > 0) {
            uint64_t eta_us = (uint64_t)(g_total - g_count) * elapsed_us / (uint64_t)g_count;
            eta_s = (int)(eta_us / 1000000);
        }
        ps4_notify("vdec2 capture: %d%%\n%d/%d ok, ~%ds left", pct, g_ok, g_count, eta_s);
        while (g_next_milestone <= pct) {
            g_next_milestone += 10;
        }
    }
}

static void try_one(const ConfigCombo& c, const char* probe) {
    OrbisVideodec2DecoderConfigInfo cfg;
    OrbisVideodec2DecoderMemoryInfo memInfo;
    memset(&cfg, 0, sizeof(cfg));
    memset(&memInfo, 0, sizeof(memInfo));

    cfg.thisSize = sizeof(cfg);
    cfg.resourceType = c.resourceType;
    cfg.codecType = c.codecType;
    cfg.profile = c.profile;
    cfg.maxLevel = c.maxLevel;
    cfg.maxFrameWidth = c.maxFrameWidth;
    cfg.maxFrameHeight = c.maxFrameHeight;
    cfg.maxDpbFrameCount = c.maxDpbFrameCount;
    cfg.decodePipelineDepth = c.decodePipelineDepth;
    cfg.computeQueue = nullptr;
    cfg.cpuAffinityMask = CPU_AFFINITY_MASK;
    cfg.cpuThreadPriority = CPU_THREAD_PRIORITY;
    cfg.optimizeProgressiveVideo = (uint8_t)c.optimizeProgressiveVideo;
    cfg.checkMemoryType = 0;
    cfg.extraConfigInfo = nullptr;

    memInfo.thisSize = sizeof(memInfo);

    int32_t r = sceVideodec2QueryDecoderMemoryInfo(&cfg, &memInfo);
    emit_entry(c, memInfo, r, probe);
    ++g_count;
    if (r == 0) {
        ++g_ok;
    } else {
        ++g_err;
    }
    maybe_progress(probe);
}

static void run_grid() {
    const size_t n_dims = sizeof(COMMON_DIMS) / sizeof(COMMON_DIMS[0]);
    const size_t n_levels = sizeof(AVC_LEVELS) / sizeof(AVC_LEVELS[0]);
    const size_t n_profiles = sizeof(AVC_PROFILES) / sizeof(AVC_PROFILES[0]);
    for (size_t pi = 0; pi < n_profiles; ++pi) {
        for (size_t li = 0; li < n_levels; ++li) {
            for (size_t di = 0; di < n_dims; ++di) {
                for (int32_t dpb = 1; dpb <= 4; ++dpb) {
                    for (int32_t pipe = 1; pipe <= 4; ++pipe) {
                        for (int32_t opt = 0; opt <= 1; ++opt) {
                            ConfigCombo c{};
                            c.resourceType = 1;
                            c.codecType = 1;
                            c.profile = AVC_PROFILES[pi];
                            c.maxLevel = AVC_LEVELS[li];
                            c.maxFrameWidth = COMMON_DIMS[di][0];
                            c.maxFrameHeight = COMMON_DIMS[di][1];
                            c.maxDpbFrameCount = dpb;
                            c.decodePipelineDepth = pipe;
                            c.optimizeProgressiveVideo = opt;
                            try_one(c, "grid");
                        }
                    }
                }
            }
        }
    }
}

static void run_width_probe() {
    for (int32_t w = WPROBE_MIN; w <= WPROBE_MAX; ++w) {
        ConfigCombo c{};
        c.resourceType = 1;
        c.codecType = 1;
        c.profile = 66;
        c.maxLevel = PROBE_LEVEL;
        c.maxFrameWidth = w;
        c.maxFrameHeight = WPROBE_HEIGHT;
        c.maxDpbFrameCount = 1;
        c.decodePipelineDepth = 1;
        c.optimizeProgressiveVideo = 0;
        try_one(c, "wprobe");
    }
}

static void run_height_probe() {
    for (int32_t h = HPROBE_MIN; h <= HPROBE_MAX; ++h) {
        ConfigCombo c{};
        c.resourceType = 1;
        c.codecType = 1;
        c.profile = 66;
        c.maxLevel = PROBE_LEVEL;
        c.maxFrameWidth = HPROBE_WIDTH;
        c.maxFrameHeight = h;
        c.maxDpbFrameCount = 1;
        c.decodePipelineDepth = 1;
        c.optimizeProgressiveVideo = 0;
        try_one(c, "hprobe");
    }
}

static void run_p10_probe() {
    const size_t n_prof = sizeof(P10_PROFILES) / sizeof(P10_PROFILES[0]);
    const size_t n_dims = sizeof(P10_DIMS) / sizeof(P10_DIMS[0]);
    for (size_t pi = 0; pi < n_prof; ++pi) {
        for (size_t di = 0; di < n_dims; ++di) {
            ConfigCombo c{};
            c.resourceType = 1;
            c.codecType = 1;
            c.profile = P10_PROFILES[pi];
            c.maxLevel = PROBE_LEVEL;
            c.maxFrameWidth = P10_DIMS[di][0];
            c.maxFrameHeight = P10_DIMS[di][1];
            c.maxDpbFrameCount = 1;
            c.decodePipelineDepth = 1;
            c.optimizeProgressiveVideo = 0;
            try_one(c, "p10");
        }
    }
}

extern "C" int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    g_start_us = sceKernelGetProcessTime();
    g_total = compute_total();

    sceKernelDebugOutText(0, "vdec2_capture starting\n");
    ps4_notify("vdec2 capture: starting\n%d configs, ~30 seconds", g_total);
    update_status_file("starting");

    g_out = fopen("/data/vdec2_capture.json", "w");
    if (!g_out) {
        sceKernelDebugOutText(0, "Failed to open /data/vdec2_capture.json\n");
        ps4_notify("vdec2 capture FAILED:\ncannot open /data/vdec2_capture.json");
        update_status_file("error: cannot open output file");
        return 1;
    }

    fputs("[\n", g_out);

    run_grid();
    run_width_probe();
    run_height_probe();
    run_p10_probe();

    fputs("\n]\n", g_out);
    fflush(g_out);
    fclose(g_out);

    update_status_file("done");

    char done_log[160];
    snprintf(done_log, sizeof(done_log),
             "vdec2_capture: DONE %d/%d ok (%d errors) -> /data/vdec2_capture.json\n", g_ok,
             g_count, g_err);
    sceKernelDebugOutText(0, done_log);

    ps4_notify("vdec2 capture DONE\n%d/%d ok (%d errors)\nFTP: /data/vdec2_capture.json", g_ok,
               g_count, g_err);

    return 0;
}
