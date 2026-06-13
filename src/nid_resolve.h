#pragma once
/* Types come from <stdint.h> which main.c includes before this header.
   Do NOT re-typedef uint32_t / uint8_t / int64_t here — that conflicts with
   the OpenOrbis SDK's <stdint.h> when building via the SDK toolchain. */
#include <stdint.h>

extern int sceKernelAllocateDirectMemory(long, long, unsigned long, unsigned long, int, unsigned long*);
extern int sceKernelMapDirectMemory(void**, unsigned long, int, int, long, unsigned long);
extern unsigned int sceKernelUsleep(unsigned int);
extern int sceKernelOpen(const char*, int, unsigned short);
extern long sceKernelRead(int, void*, unsigned long);
extern int sceKernelClose(int);
extern long sceKernelLseek(int, long, int);
extern int sceVideoOutOpen(int, int, int, void*);
extern void sceVideoOutSetBufferAttribute(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
extern int sceVideoOutRegisterBuffers(int, int, void *const*, int, const void*);
extern int sceVideoOutSubmitFlip(int, int, uint32_t, int64_t);
extern void sceVideoOutSetFlipRate(int, int);
/* Flip status — OpenOrbis canonical layout (verified). flipArg lets us wait
   for a SPECIFIC frame's flip to complete (the proper PS4 sync), and
   numFlipPending bounds the queue. */
typedef struct {
    uint64_t num;
    uint64_t ptime;
    uint64_t stime;
    int64_t  flipArg;
    uint64_t reserved[2];
    int32_t  numGpuFlipPending;
    int32_t  numFlipPending;
    int32_t  currentBuffer;
    uint32_t reserved1;
} OrbisVideoOutFlipStatus;
extern int sceVideoOutGetFlipStatus(int, OrbisVideoOutFlipStatus*);
extern int sceVideoOutUnregisterBuffers(int, int);
extern int sceVideoOutClose(int);
/* System service event — used to detect the quit/terminate request so we can
   exit cleanly instead of letting the OS time out (~1 min) and crash.
   The event struct is large (8192-byte SDK layout); we only read eventType,
   the first int32. ReceiveEvent returns 0 when an event was dequeued. */
extern int sceSystemServiceReceiveEvent(void* event);
extern int sceGnmSubmitCommandBuffers(uint32_t, void**, uint32_t*, uint32_t*, uint32_t);
/* Canonical submit+flip: patches the prepareFlip packet at the end of the last
   DCB so the GPU writes the VO buffer label and flips at end-of-pipe (the
   handshake the standalone sceVideoOutSubmitFlip skipped). */
extern int sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void** dcb_addrs,
                                             uint32_t* dcb_sizes, void** ccb_addrs,
                                             uint32_t* ccb_sizes, int vo_handle,
                                             int buf_idx, uint32_t flip_mode,
                                             int64_t flip_arg);
extern int sceGnmSubmitDone(void);
extern int printf(const char*, ...);
extern int scePadInit(void);
extern int scePadOpen(int, int, int, void*);
extern int scePadRead(int, void*, int);
extern int scePadReadState(int, void*);
extern int sceUserServiceInitialize(void*);
extern int sceUserServiceGetInitialUser(int*);
typedef struct { uint32_t format; uint32_t tiling; uint32_t aspect; uint32_t width; uint32_t height; uint32_t pitch; } SceVideoOutBufferAttribute;
struct OrbisPadData {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t _pad0[2];
    float quat[4], accel[3], angvel[3];
    uint8_t _rest[200];
};
#define PAD_L3 0x0002
#define PAD_R3 0x0004
#define PAD_OPTIONS 0x0008
#define PAD_UP 0x0010
#define PAD_RIGHT 0x0020
#define PAD_DOWN 0x0040
#define PAD_LEFT 0x0080
#define PAD_L1 0x0400
#define PAD_R1 0x0800
#define PAD_TRI 0x1000
#define PAD_CROSS 0x4000

/* Memory freeing */
extern int sceKernelMunmap(void *addr, unsigned long len);
extern int sceKernelReleaseDirectMemory(long phys_addr, unsigned long len);
