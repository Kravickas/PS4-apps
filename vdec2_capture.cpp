// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// vdec2_capture: PS4 homebrew that calls sceVideodec2QueryDecoderMemoryInfo
// across a comprehensive grid of decoder configs and writes results to
// /data/vdec2_capture.json.
//
// This source is intentionally self-contained — it does NOT include any
// <orbis/...> headers. The OpenOrbis SDK's Videodec2.h only ships a stub
// declaration `void sceVideodec2QueryDecoderMemoryInfo();`, so we declare
// the struct layouts and function prototype ourselves here. The layouts
// are RE'd from libSceVideodec2.sprx (FW 9.00) and verified against shadPS4.
//
// After building and installing the .pkg on a jailbroken PS4:
//   1. Run from the homebrew menu — exits when complete (~30-90s)
//   2. FTP /data/vdec2_capture.json off the console
//   3. Feed it back to the shadPS4 RE pipeline

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ---------- PS4 system symbols (link against libkernel + libSceVideodec2) ----------

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

// ---------- Capture grid ----------

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

static FILE* g_out = nullptr;
static int g_count = 0;
static int g_ok = 0;
static int g_err = 0;

static void log_msg(const char* msg) {
    sceKernelDebugOutText(0, msg);
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

extern "C" int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    log_msg("vdec2_capture starting\n");

    g_out = fopen("/data/vdec2_capture.json", "w");
    if (!g_out) {
        log_msg("Failed to open /data/vdec2_capture.json\n");
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
                            if ((g_count % 200) == 0) {
                                char msg[80];
                                snprintf(msg, sizeof(msg),
                                         "vdec2_capture: %d/%d ok\n",
                                         g_ok, g_count);
                                log_msg(msg);
                            }
                        }
                    }
                }
            }
        }
    }

    fputs("\n]\n", g_out);
    fflush(g_out);
    fclose(g_out);

    char done[120];
    snprintf(done, sizeof(done),
             "vdec2_capture: DONE %d/%d ok (%d errors) -> /data/vdec2_capture.json\n",
             g_ok, g_count, g_err);
    log_msg(done);
    return 0;
}
