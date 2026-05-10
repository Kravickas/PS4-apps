// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// hello_world: minimal PS4 homebrew used to verify the build pipeline.
//
// This source uses ONLY libkernel functions — NO libc functions
// (no fopen, fprintf, memset, strncpy, etc.). That means the resulting
// eboot.bin does not depend on libc.prx and can launch even if the .pkg
// doesn't bundle libc.prx in sce_module/.
//
// If THIS can't launch on a PS4, the build pipeline (create-fself,
// create-gp4, PkgTool.Core, param.sfo entries, content_id, icon0.png) is
// the problem, independent of any application code.
//
// On launch you should see a notification banner top-right reading
// "Hello from shadPS4 build pipeline!"

#include <stddef.h>
#include <stdint.h>

// ---- libkernel imports (only thing this homebrew links) ------------------

extern "C" int sceKernelDebugOutText(int channel, const char* text);
extern "C" int sceKernelSendNotificationRequest(int api, void* request,
                                                size_t size, int blocking);

// ---- PS4 notification struct (RE'd by OSM-Made/PS4-Notify) ---------------

#pragma pack(push, 1)
struct OrbisNotificationRequest {
    uint32_t type;             // 0x00 — 0 = Message
    uint32_t reqId;            // 0x04
    uint32_t priority;         // 0x08
    uint32_t msgId;            // 0x0c
    uint32_t targetId;         // 0x10 — -1 = all users
    uint32_t userId;           // 0x14
    uint32_t deviceId;         // 0x18
    uint32_t addressingUserId; // 0x1c
    uint32_t appId;            // 0x20
    uint32_t errorNumber;      // 0x24
    uint32_t attribute;        // 0x28
    uint8_t  hasIcon;          // 0x2c
    char     message[0x400];   // 0x2d
    char     iconImageUri[0x800]; // 0x42d
    char     padding[3];       // 0xc2d -> total size 0xc30
};
#pragma pack(pop)
static_assert(sizeof(OrbisNotificationRequest) == 0xC30,
              "OrbisNotificationRequest must be exactly 0xC30 bytes");

// ---- Tiny libc-free helpers ----------------------------------------------

static void zero_bytes(void* dst, size_t n) {
    uint8_t* p = static_cast<uint8_t*>(dst);
    for (size_t i = 0; i < n; ++i) {
        p[i] = 0;
    }
}

static void copy_cstring(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (dst_size == 0) {
        return;
    }
    while (src[i] != '\0' && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

// ---- main -----------------------------------------------------------------

extern "C" int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    sceKernelDebugOutText(0, "hello_world: started\n");

    OrbisNotificationRequest req;
    zero_bytes(&req, sizeof(req));
    req.type = 0;
    req.targetId = -1;
    req.hasIcon = 1;
    copy_cstring(req.iconImageUri, sizeof(req.iconImageUri),
                 "cxml://psnotification/tex_default_icon_notification");
    copy_cstring(req.message, sizeof(req.message),
                 "Hello from shadPS4 build pipeline!\n"
                 "If you see this, the build is working.");

    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
    sceKernelDebugOutText(0, "hello_world: notification sent, exiting\n");
    return 0;
}
