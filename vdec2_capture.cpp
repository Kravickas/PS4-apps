// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// vdec2_capture: PS4 homebrew that calls sceVideodec2QueryDecoderMemoryInfo across
// a comprehensive grid of decoder configs and writes results to /data/vdec2_capture.json.
//
// Build with OpenOrbis toolchain. Run on jailbroken PS4 (FW 9.00+ recommended).
// FTP /data/vdec2_capture.json off the console afterward and feed it back to the
// shadPS4 RE pipeline to derive (or directly bundle) the size formulas.
//
// Why this exists:
//   sceVideodec2QueryDecoderMemoryInfo internally calls sceVdecCoreQueryInstanceSize
//   (verified via static RE in libSceVideodec2 0x325a, see RE_progress.md). The
//   formula sub_3e00 -> sub_19240 -> sub_180c0 chain consumes a per-codec internal
//   cfg with non-trivial layout transformations through sub_1070, plus a per-frame
//   computation in sub_18b20 that depends on cfg byte fields [+0x4b..+0x56]. The
//   user-facing-to-internal cfg mapping has not been fully traced.
//
//   Rather than continuing speculative formula derivation, this tool captures the
//   real PS4 outputs directly. The captured dataset can then either:
//     (a) be used to derive the formula via larger-scale empirical regression, or
//     (b) be bundled as a lookup table in shadPS4 for byte-perfect values for
//         configs games actually use.
//
// Usage:
//   1. Build with `make` in this directory (requires OO_PS4_TOOLCHAIN env var)
//   2. Send the resulting .pkg to PS4 via Remote PKG installer
//   3. Run from the homebrew menu — the homebrew prints progress and exits when done
//   4. FTP /data/vdec2_capture.json off the PS4

#include <orbis/libkernel.h>
#include <orbis/Videodec2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Minimal AVC profile/level enumeration matching the JSON dataset structure.
// Add HEVC/VP9 grids if those codecs are needed.
struct ConfigCombo {
    int32_t resourceType;     // 1 = AVC, 0x12384 = HEVC, 0xb6c8 = VP9
    int32_t codecType;        // 1 for AVC; specific values per codec
    int32_t profile;          // 66/77/100 for AVC
    int32_t maxLevel;         // 10..52
    int32_t maxFrameWidth;    // -1 for auto
    int32_t maxFrameHeight;   // -1 for auto
    int32_t maxDpbFrameCount; // 1..16
    int32_t decodePipelineDepth; // 1..8
    int32_t optimizeProgressiveVideo; // 0 or 1
};

// AVC profiles in canonical PS4 set
static const int32_t AVC_PROFILES[] = {66, 77, 100};
// AVC levels matching JSON
static const int32_t AVC_LEVELS[] = {10, 11, 12, 13, 20, 21, 22, 30, 31, 32,
                                     40, 41, 42, 50, 51, 52};
// Common (W, H) pairs games actually use
static const int32_t COMMON_DIMS[][2] = {
    {-1, -1},     // auto-dim
    {160, 128},   // PS4 QA test minimum
    {176, 144},   // QCIF
    {352, 288},   // CIF
    {640, 480},   // VGA
    {720, 480},   // SD-NTSC
    {720, 576},   // SD-PAL
    {1280, 720},  // HD720
    {1920, 1080}, // HD1080  <-- TLOU likely uses this
    {2560, 1440}, // QHD
    {3840, 2160}, // 4K
    {4096, 2160}, // DCI 4K
    {4096, 2176}, // PS4 4K cap
};

static FILE* g_out = NULL;
static int g_count = 0;

static void emit_entry(const ConfigCombo& c, const OrbisVideodec2DecoderMemoryInfo& m,
                       int32_t status) {
    if (status != 0) {
        fprintf(g_out, "  {\"status\": \"error 0x%08x\", \"config\": {", (unsigned)status);
    } else {
        fprintf(g_out, "  {\"decoderConfigInfo\": {");
    }
    fprintf(g_out,
            "\"resourceType\": \"%d\", \"codecType\": \"%d\", \"profile\": \"%d\", "
            "\"maxLevel\": \"%d\", \"maxFrameWidth\": \"%d\", \"maxFrameHeight\": \"%d\", "
            "\"maxDpbFrameCount\": \"%d\", \"decodePipelineDepth\": \"%d\", "
            "\"cpuAffinityMask\": \"63\", \"cpuThreadPriority\": \"700\", "
            "\"optimizeProgressiveVideo\": \"%d\"}",
            c.resourceType, c.codecType, c.profile, c.maxLevel, c.maxFrameWidth,
            c.maxFrameHeight, c.maxDpbFrameCount, c.decodePipelineDepth,
            c.optimizeProgressiveVideo);
    if (status == 0) {
        fprintf(g_out,
                ", \"decoderMemoryInfo\": {\"cpuMemorySize\": \"%lu\", "
                "\"gpuMemorySize\": \"%lu\", \"cpuGpuMemorySize\": \"%lu\", "
                "\"maxFrameBufferSize\": \"%lu\", \"frameBufferAlignment\": \"%lu\"}",
                (unsigned long)m.cpuMemorySize, (unsigned long)m.gpuMemorySize,
                (unsigned long)m.cpuGpuMemorySize, (unsigned long)m.maxFrameBufferSize,
                (unsigned long)m.frameBufferAlignment);
    }
    fprintf(g_out, "}%s\n", "");
    fflush(g_out);
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
    cfg.computeQueue = 0;
    cfg.cpuAffinityMask = 0x3F;
    cfg.cpuThreadPriority = 700;
    cfg.optimizeProgressiveVideo = (uint8_t)c.optimizeProgressiveVideo;
    cfg.checkMemoryType = 0;
    cfg.extraConfigInfo = NULL;

    memInfo.thisSize = sizeof(memInfo);

    int32_t r = sceVideodec2QueryDecoderMemoryInfo(&cfg, &memInfo);
    if (g_count > 0) {
        fputs(",", g_out);
    }
    emit_entry(c, memInfo, r);
    ++g_count;
}

extern "C" int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    sceKernelDebugOutText(0, "vdec2_capture starting\n");

    g_out = fopen("/data/vdec2_capture.json", "w");
    if (!g_out) {
        sceKernelDebugOutText(0, "Failed to open /data/vdec2_capture.json\n");
        return 1;
    }

    fprintf(g_out, "[\n");

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
                            if ((g_count % 100) == 0) {
                                char msg[64];
                                snprintf(msg, sizeof(msg),
                                         "vdec2_capture: %d entries\n", g_count);
                                sceKernelDebugOutText(0, msg);
                            }
                        }
                    }
                }
            }
        }
    }

    fprintf(g_out, "\n]\n");
    fclose(g_out);

    char done[80];
    snprintf(done, sizeof(done), "vdec2_capture: DONE, %d entries written\n", g_count);
    sceKernelDebugOutText(0, done);
    return 0;
}
