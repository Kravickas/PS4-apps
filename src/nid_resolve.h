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
extern int sceGnmSubmitCommandBuffers(uint32_t, void**, uint32_t*, uint32_t*, uint32_t);
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
