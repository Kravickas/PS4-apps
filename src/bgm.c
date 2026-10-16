/* Looping background music: a 16-bit stereo 48 kHz PCM WAV, played on the
   main audio port by its own thread. sceAudioOutOutput blocks for one grain
   (BGM_GRAIN frames = 5.33 ms), so the thread paces itself; the two grain
   buffers alternate because the port may read the previous one until the next
   call returns. The loop seam is made seamless offline (tools/make_bgm.py). */
#include <stdint.h>
#include "bgm.h"
#include "nid_resolve.h"

/* libSceAudioOut, as declared in the OpenOrbis toolchain (orbis/AudioOut.h). */
extern int32_t sceAudioOutInit(void);
extern int32_t sceAudioOutOpen(int32_t user_id, int32_t port_type, int32_t index, uint32_t length,
                               uint32_t sample_rate, uint32_t param_type);
extern int32_t sceAudioOutOutput(int32_t handle, const void* ptr);

#define BGM_USER_SYSTEM 0xFF    /* ORBIS_USER_SERVICE_USER_ID_SYSTEM */
#define BGM_PORT_MAIN 0         /* ORBIS_AUDIO_OUT_PORT_TYPE_MAIN */
#define BGM_FORMAT_S16_STEREO 1 /* ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO */
#define BGM_RATE 48000          /* the only rate sceAudioOutOpen accepts */
#define BGM_GRAIN 256           /* frames per output: a multiple of 256, at most 2048 */
#define BGM_ALREADY_INIT ((int32_t)0x8026000E) /* ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT */
#define BGM_MAX_FILE (64UL << 20)

static struct {
    const int16_t* pcm;
    uint32_t frames;
    int32_t handle;
} g_bgm;
static int16_t g_bgm_buf[2][BGM_GRAIN * 2] __attribute__((aligned(64)));

static void* bgm_thread(void* arg) {
    (void)arg;
    uint32_t pos = 0;
    int k = 0;
    for (;;) {
        int16_t* o = g_bgm_buf[k];
        for (int i = 0; i < BGM_GRAIN; i++) {
            o[2 * i] = g_bgm.pcm[2 * pos];
            o[2 * i + 1] = g_bgm.pcm[2 * pos + 1];
            if (++pos == g_bgm.frames)
                pos = 0;
        }
        if (sceAudioOutOutput(g_bgm.handle, o) < 0)
            break;
        k ^= 1;
    }
    return 0;
}

static uint32_t bgm_u32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t bgm_u16(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}
static int bgm_tag(const unsigned char* p, const char* t) {
    return p[0] == (unsigned char)t[0] && p[1] == (unsigned char)t[1] &&
           p[2] == (unsigned char)t[2] && p[3] == (unsigned char)t[3];
}

int bgm_start(const char* path, bgm_alloc_fn alloc) {
    int fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0);
    if (fd < 0)
        return -1;
    long size = sceKernelLseek(fd, 0, 2 /* SEEK_END */);
    sceKernelLseek(fd, 0, 0 /* SEEK_SET */);
    if (size < 44 || (unsigned long)size > BGM_MAX_FILE) {
        sceKernelClose(fd);
        return -2;
    }
    unsigned char* file = (unsigned char*)alloc((unsigned long)size, 64);
    if (!file) {
        sceKernelClose(fd);
        return -3;
    }
    long got = 0;
    while (got < size) {
        long r = sceKernelRead(fd, file + got, (unsigned long)(size - got));
        if (r <= 0)
            break;
        got += r;
    }
    sceKernelClose(fd);
    if (got != size)
        return -4;

    /* RIFF / WAVE: "fmt " must be PCM (1), 2 channels, 48000 Hz, 16 bits. */
    if (!bgm_tag(file, "RIFF") || !bgm_tag(file + 8, "WAVE"))
        return -5;
    const unsigned char* data = 0;
    uint32_t data_size = 0;
    int fmt_ok = 0;
    long p = 12;
    while (p + 8 <= size) {
        uint32_t len = bgm_u32(file + p + 4);
        const unsigned char* c = file + p + 8;
        if ((unsigned long)len > (unsigned long)(size - p - 8))
            return -6;
        if (bgm_tag(file + p, "fmt "))
            fmt_ok = len >= 16 && bgm_u16(c) == 1 && bgm_u16(c + 2) == 2 &&
                     bgm_u32(c + 4) == BGM_RATE && bgm_u16(c + 14) == 16;
        else if (bgm_tag(file + p, "data")) {
            data = c;
            data_size = len;
        }
        p += 8 + (long)len + (long)(len & 1);
    }
    if (!fmt_ok)
        return -7;
    if (!data || data_size < BGM_GRAIN * 4 || ((uintptr_t)data & 1))
        return -8;
    g_bgm.pcm = (const int16_t*)data;
    g_bgm.frames = data_size / 4;

    int32_t r = sceAudioOutInit();
    if (r < 0 && r != BGM_ALREADY_INIT)
        return r;
    int32_t h = sceAudioOutOpen(BGM_USER_SYSTEM, BGM_PORT_MAIN, 0, BGM_GRAIN, BGM_RATE,
                                BGM_FORMAT_S16_STEREO);
    if (h < 0)
        return h;
    g_bgm.handle = h;
    void* thr = 0;
    if (scePthreadCreate(&thr, 0, bgm_thread, 0, "bgm") != 0)
        return -9;
    return 0;
}
