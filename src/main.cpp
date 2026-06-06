// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// vdec2_capture: PS4 homebrew that calls sceVideodec2QueryDecoderMemoryInfo
// across a comprehensive grid of decoder configs and writes results to
// /data/vdec2_capture.json.
//
// User feedback while running:
//   - PS4 toast notification at start ("Starting capture...")
//   - PS4 toast notification at end with results ("DONE x/y ok ...")
//   - Live status file at /data/vdec2_capture.status updated every 200
//     iterations (FTP-pollable in real time)
//
// This source is intentionally self-contained — it does NOT include any
// <orbis/...> headers. The OpenOrbis SDK's Videodec2.h only ships a stub
// declaration `void sceVideodec2QueryDecoderMemoryInfo();`, so the struct
// layouts and function prototype are declared inline. Layouts RE'd from
// libSceVideodec2.sprx (FW 9.00) and verified against shadPS4.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- PS4 system symbols (link against libkernel + libSceVideodec2) --

extern "C" int sceKernelDebugOutText(int channel, const char* text);

// ---------- libSceVideodec2 ABI (RE'd, not in OpenOrbis headers) ----------
//
// Both structs are exactly 0x48 bytes. The PS4 firmware validates this
// strictly at libSceVideodec2 0xba1/0xba6 (`cmp [r12], 0x48; jne err`).

struct OrbisVideodec2DecoderConfigInfo {
    uint64_t thisSize;                 // 0x00 — must be 0x48
    uint32_t resourceType;             // 0x08
    uint32_t codecType;                // 0x0c — 1 = AVC
    uint32_t profile;                  // 0x10
    uint32_t maxLevel;                 // 0x14
    int32_t  maxFrameWidth;            // 0x18 — -1 for auto-dim
    int32_t  maxFrameHeight;           // 0x1c — -1 for auto-dim
    int32_t  maxDpbFrameCount;         // 0x20
    uint32_t decodePipelineDepth;      // 0x24
    void*    computeQueue;             // 0x28
    uint64_t cpuAffinityMask;          // 0x30
    int32_t  cpuThreadPriority;        // 0x38
    uint8_t  optimizeProgressiveVideo; // 0x3c
    uint8_t  checkMemoryType;          // 0x3d
    uint8_t  reserved0;                // 0x3e
    uint8_t  reserved1;                // 0x3f
    void*    extraConfigInfo;          // 0x40
};
static_assert(sizeof(OrbisVideodec2DecoderConfigInfo) == 0x48,
              "DecoderConfigInfo must be 0x48 bytes");

struct OrbisVideodec2DecoderMemoryInfo {
    uint64_t thisSize;             // 0x00 — must be 0x48
    uint64_t cpuMemorySize;        // 0x08
    void*    cpuMemory;            // 0x10
    uint64_t gpuMemorySize;        // 0x18
    void*    gpuMemory;            // 0x20
    uint64_t cpuGpuMemorySize;     // 0x28
    void*    cpuGpuMemory;         // 0x30
    uint64_t maxFrameBufferSize;   // 0x38
    uint32_t frameBufferAlignment; // 0x40
    uint32_t reserved0;            // 0x44
};
static_assert(sizeof(OrbisVideodec2DecoderMemoryInfo) == 0x48,
              "DecoderMemoryInfo must be 0x48 bytes");

extern "C" int32_t sceVideodec2QueryDecoderMemoryInfo(
    const OrbisVideodec2DecoderConfigInfo* decoderConfigInfo,
    OrbisVideodec2DecoderMemoryInfo* decoderMemoryInfo);

// ---------- PS4 toast notification (libkernel) ----------------------------
//
// Layout RE'd by OSM-Made/PS4-Notify. Total size must be exactly 0xC30 bytes
// (3120) — the kernel rejects anything else. Verified by static_assert.
// Channel 0 = ToastPopup (banner top-right), channel 1 = NotifyDatabase.

#pragma pack(push, 1)
struct OrbisNotificationRequest {
    uint32_t type;             // 0x00 — 0 = Message
    uint32_t reqId;            // 0x04
    uint32_t priority;         // 0x08 — 0 = Default
    uint32_t msgId;            // 0x0c
    uint32_t targetId;         // 0x10 — -1 = all users
    uint32_t userId;           // 0x14
    uint32_t deviceId;         // 0x18
    uint32_t addressingUserId; // 0x1c
    uint32_t appId;            // 0x20
    uint32_t errorNumber;      // 0x24
    uint32_t attribute;        // 0x28 — 0 = no special attrs
    uint8_t  hasIcon;          // 0x2c
    char     message[0x400];   // 0x2d (1024 bytes)
    char     iconImageUri[0x800]; // 0x42d (2048 bytes)
    char     padding[3];       // 0xc2d — pad to 0xc30
};
#pragma pack(pop)
static_assert(sizeof(OrbisNotificationRequest) == 0xC30,
              "OrbisNotificationRequest must be exactly 0xC30 bytes");

extern "C" int32_t sceKernelSendNotificationRequest(int32_t api, void* request,
                                                    size_t size, int32_t blocking);

static void ps4_notify(const char* fmt, ...) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;        // Message
    req.targetId = -1;   // all users
    req.hasIcon = 1;
    // tex_default_icon_notification = round 'i' speech bubble
    strncpy(req.iconImageUri,
            "cxml://psnotification/tex_default_icon_notification",
            sizeof(req.iconImageUri) - 1);

    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message) - 1, fmt, args);
    va_end(args);

    sceKernelSendNotificationRequest(0 /* ToastPopup */, &req, sizeof(req), 0);
}

// ---------- Capture grid --------------------------------------------------

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
static const int32_t AVC_LEVELS[] = {10, 11, 12, 13, 20, 21, 22, 30, 31, 32,
                                     40, 41, 42, 50, 51, 52};
static const int32_t COMMON_DIMS[][2] = {
    {-1, -1},     // auto-dim: kernel uses 4096x4096 for L=52 else 4096x2176
    {160, 128},   {176, 144},   {352, 288},   {640, 480},
    {720, 480},   {720, 576},   {1280, 720},  {1920, 1080},
    {2560, 1440}, {3840, 2160}, {4096, 2160}, {4096, 2176},
};

// Step-function probes, appended after the grid; reuse the proven try_one().
static const int32_t PROBE_LEVEL = 51;
static const int32_t WPROBE_MIN = 16;
static const int32_t WPROBE_MAX = 560;
static const int32_t WPROBE_HEIGHT = 272;
static const int32_t HPROBE_MIN = 16;
static const int32_t HPROBE_MAX = 272;
static const int32_t HPROBE_WIDTH = 512;
static const int32_t P10_PROFILES[] = {110, 122, 244};
static const int32_t P10_DIMS[][2] = {{1920, 1080}, {1280, 720}};

static FILE* g_out = nullptr;
static int g_count = 0;
static int g_ok = 0;
static int g_err = 0;

static void update_status_file(const char* phase) {
    FILE* sf = fopen("/data/vdec2_capture.status", "w");
    if (!sf) {
        return;
    }
    fprintf(sf,
            "phase: %s\n"
            "processed: %d\n"
            "ok: %d\n"
            "errors: %d\n"
            "output: /data/vdec2_capture.json\n",
            phase, g_count, g_ok, g_err);
    fclose(sf);
}

static void emit_entry(const ConfigCombo& c, const OrbisVideodec2DecoderMemoryInfo& m,
                       int32_t status) {
    if (g_count > 0) {
        fputs(",\n", g_out);
    }
    fputs("  {", g_out);
    if (status != 0) {
        fprintf(g_out, "\"status\":\"error 0x%08x\",", (unsigned)status);
    }
    fprintf(g_out,
            "\"decoderConfigInfo\":{"
            "\"resourceType\":\"%d\",\"codecType\":\"%d\","
            "\"profile\":\"%d\",\"maxLevel\":\"%d\","
            "\"maxFrameWidth\":\"%d\",\"maxFrameHeight\":\"%d\","
            "\"maxDpbFrameCount\":\"%d\",\"decodePipelineDepth\":\"%d\","
            "\"cpuAffinityMask\":\"63\",\"cpuThreadPriority\":\"700\","
            "\"optimizeProgressiveVideo\":\"%d\"}",
            c.resourceType, c.codecType, c.profile, c.maxLevel,
            c.maxFrameWidth, c.maxFrameHeight, c.maxDpbFrameCount,
            c.decodePipelineDepth, c.optimizeProgressiveVideo);
    if (status == 0) {
        fprintf(g_out,
                ",\"decoderMemoryInfo\":{"
                "\"cpuMemorySize\":\"%llu\","
                "\"gpuMemorySize\":\"%llu\","
                "\"cpuGpuMemorySize\":\"%llu\","
                "\"maxFrameBufferSize\":\"%llu\","
                "\"frameBufferAlignment\":\"%u\"}",
                (unsigned long long)m.cpuMemorySize,
                (unsigned long long)m.gpuMemorySize,
                (unsigned long long)m.cpuGpuMemorySize,
                (unsigned long long)m.maxFrameBufferSize,
                (unsigned)m.frameBufferAlignment);
    }
    fputs("}", g_out);
}

static void try_one(const ConfigCombo& c) {
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
    cfg.cpuAffinityMask = 0x3F;
    cfg.cpuThreadPriority = 700;
    cfg.optimizeProgressiveVideo = (uint8_t)c.optimizeProgressiveVideo;
    cfg.checkMemoryType = 0;
    cfg.extraConfigInfo = nullptr;

    memInfo.thisSize = sizeof(memInfo);

    int32_t r = sceVideodec2QueryDecoderMemoryInfo(&cfg, &memInfo);
    emit_entry(c, memInfo, r);
    ++g_count;
    if (r == 0) {
        ++g_ok;
    } else {
        ++g_err;
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
        try_one(c);
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
        try_one(c);
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
            try_one(c);
        }
    }
}

extern "C" int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    sceKernelDebugOutText(0, "vdec2_capture starting\n");
    ps4_notify("vdec2 capture: starting\nthis takes ~30 seconds");
    update_status_file("starting");

    g_out = fopen("/data/vdec2_capture.json", "w");
    if (!g_out) {
        sceKernelDebugOutText(0, "Failed to open /data/vdec2_capture.json\n");
        ps4_notify("vdec2 capture FAILED:\ncannot open /data/vdec2_capture.json");
        update_status_file("error: cannot open output file");
        return 1;
    }

    fputs("[\n", g_out);

    const size_t n_dims = sizeof(COMMON_DIMS) / sizeof(COMMON_DIMS[0]);
    const size_t n_levels = sizeof(AVC_LEVELS) / sizeof(AVC_LEVELS[0]);
    const size_t n_profiles = sizeof(AVC_PROFILES) / sizeof(AVC_PROFILES[0]);

    // AVC grid: profile x level x (W,H) x dpb x pipe x opt
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
                            try_one(c);
                            // Update status file every 200 entries so user
                            // can monitor real-time progress via FTP poll.
                            if ((g_count % 200) == 0) {
                                update_status_file("running");
                                char msg[80];
                                snprintf(msg, sizeof(msg),
                                         "vdec2_capture: %d/%d ok\n",
                                         g_ok, g_count);
                                sceKernelDebugOutText(0, msg);
                            }
                        }
                    }
                }
            }
        }
    }

    run_width_probe();
    run_height_probe();
    run_p10_probe();

    fputs("\n]\n", g_out);
    fflush(g_out);
    fclose(g_out);

    update_status_file("done");

    char done_log[160];
    snprintf(done_log, sizeof(done_log),
             "vdec2_capture: DONE %d/%d ok (%d errors) -> /data/vdec2_capture.json\n",
             g_ok, g_count, g_err);
    sceKernelDebugOutText(0, done_log);

    ps4_notify("vdec2 capture DONE\n%d/%d ok (%d errors)\nFTP: /data/vdec2_capture.json",
               g_ok, g_count, g_err);

    return 0;
}
