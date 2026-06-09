// ============================================================================
// PS4 GPU/CPU Race Condition Test Suite - FULL EDITION (single-file)
// 28 tests covering RAW/WAR/WAW hazards. Passes on real PS4; failures on
// shadps4 pinpoint emulator bugs. Results -> /data/results.txt.
//
// Fixes vs multi-file version:
//   * Durable logging: fsync after EVERY write, so a crash never loses results.
//   * Output path /data/ first (writable, reliably visible) then /temp0/.
//   * Two-compute-queue test (13) runs LAST: its 2nd ASC DingDong crashes
//     shadps4, so running it last preserves all other results on disk.
//   * ComputeQueue::begin() guards ring write position against underflow.
// ============================================================================

#include <stdint.h>
#include <stddef.h>

typedef long off_t;

extern "C" {
    int32_t sceKernelAllocateDirectMemory(long search_start, long search_end, size_t len,
                                           size_t alignment, int memory_type, long* phys_addr_out);
    int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags,
                                     long phys_addr, size_t alignment);
    int  sceKernelUsleep(unsigned int usec);
    int  sceKernelOpen(const char* path, int flags, unsigned short mode);
    long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int  sceKernelClose(int fd);
    int  sceKernelFsync(int fd);
    int  sceGnmSubmitCommandBuffers(uint32_t count, const uint32_t* dcb_addrs[],
                                    uint32_t* dcb_sizes, const uint32_t* ccb_addrs[],
                                    uint32_t* ccb_sizes);
    int  sceGnmSubmitDone();
    int  sceGnmMapComputeQueue(uint32_t pipe_id, uint32_t queue_id, uintptr_t ring_base_addr,
                               uint32_t ring_size_dw, uint32_t* read_ptr_addr);
    void sceGnmDingDong(uint32_t gnm_vqid, uint32_t next_offs_dw);
    int  sceGnmUnmapComputeQueue(uint32_t gnm_vqid);
    void sceSystemServiceLoadExec(const char* path, const char* args[]);
    long sceKernelGetDirectMemorySize(void);
    int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, const void* param);
    void    sceVideoOutSetBufferAttribute(void* attr, uint32_t format, uint32_t tmode, uint32_t aspect,
                                          uint32_t w, uint32_t h, uint32_t pitch);
    int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex, void* const* addrs,
                                       int32_t num, const void* attr);
    int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufIndex, uint32_t flipMode, int64_t flipArg);
    int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
    int32_t sceVideoOutGetFlipStatus(int32_t handle, void* status);
    int32_t scePadInit(void);
    int32_t scePadOpen(int32_t userID, int32_t type, int32_t index, void* param);
    int32_t scePadReadState(int32_t handle, void* data);
    int32_t sceUserServiceInitialize(void* params);
    int32_t sceUserServiceGetInitialUser(int32_t* userId);
}

static void* my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d; while (n--) *p++ = (unsigned char)v; return d;
}
#define memset my_memset

// ---- On-screen results via Video Out --------------------------------------
// Verified vs OpenOrbis SDK (orbis/VideoOut.h, _types/video.h) and shadPS4
// videoout/buffer.h: pixel format A8R8G8B8_SRGB = 0x80000000 (encoded 0x80RRGGBB);
// tiling LINEAR = 1 (CPU-written linear framebuffer). Font: public-domain font8x8.
// NOTE: the link step must include -lSceVideoOut.
struct VideoBufAttr {            // OrbisVideoOutBufferAttribute
    int32_t format; int32_t tmode; int32_t aspect;
    uint32_t width; uint32_t height; uint32_t pitch; uint64_t reserved[2];
};
struct VideoFlipStatus {         // OrbisVideoOutFlipStatus (leading fields)
    uint64_t num, ptime, stime; int64_t flipArg; uint64_t reserved[2];
    int32_t numGpuFlipPending, numFlipPending, currentBuffer; uint32_t r1;
};

static const unsigned char FONT[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x20  
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  // 0x21 !
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x22 "
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},  // 0x23 #
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  // 0x24 $
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  // 0x25 %
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  // 0x26 &
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x27 '
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  // 0x28 (
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  // 0x29 )
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  // 0x2A *
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  // 0x2B +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // 0x2C ,
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  // 0x2D -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // 0x2E .
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  // 0x2F /
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  // 0x30 0
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  // 0x31 1
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  // 0x32 2
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  // 0x33 3
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  // 0x34 4
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  // 0x35 5
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  // 0x36 6
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  // 0x37 7
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  // 0x38 8
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  // 0x39 9
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // 0x3A :
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // 0x3B ;
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  // 0x3C <
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  // 0x3D =
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  // 0x3E >
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  // 0x3F ?
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  // 0x40 @
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  // 0x41 A
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  // 0x42 B
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  // 0x43 C
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  // 0x44 D
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  // 0x45 E
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  // 0x46 F
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  // 0x47 G
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  // 0x48 H
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 0x49 I
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  // 0x4A J
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  // 0x4B K
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  // 0x4C L
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  // 0x4D M
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  // 0x4E N
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  // 0x4F O
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  // 0x50 P
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  // 0x51 Q
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  // 0x52 R
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  // 0x53 S
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 0x54 T
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  // 0x55 U
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // 0x56 V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  // 0x57 W
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  // 0x58 X
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  // 0x59 Y
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  // 0x5A Z
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},  // 0x5B [
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},  // 0x5C (backslash)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},  // 0x5D ]
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},  // 0x5E ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  // 0x5F _
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x60 `
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},  // 0x61 a
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},  // 0x62 b
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},  // 0x63 c
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},  // 0x64 d
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},  // 0x65 e
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},  // 0x66 f
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // 0x67 g
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},  // 0x68 h
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 0x69 i
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},  // 0x6A j
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},  // 0x6B k
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // 0x6C l
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},  // 0x6D m
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},  // 0x6E n
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},  // 0x6F o
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},  // 0x70 p
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},  // 0x71 q
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},  // 0x72 r
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},  // 0x73 s
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},  // 0x74 t
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},  // 0x75 u
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // 0x76 v
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},  // 0x77 w
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},  // 0x78 x
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // 0x79 y
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},  // 0x7A z
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},  // 0x7B {
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // 0x7C |
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},  // 0x7D }
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 0x7E ~
};

#define SCR_MAXLINES 160
// DualShock4 button masks (verified vs orbis/_types/pad.h)
#define PAD_UP       0x0010u
#define PAD_RIGHT    0x0020u
#define PAD_DOWN     0x0040u
#define PAD_LEFT     0x0080u
#define PAD_L2       0x0100u
#define PAD_R2       0x0200u
#define PAD_L1       0x0400u
#define PAD_R1       0x0800u
#define PAD_TRIANGLE 0x1000u
#define PAD_CIRCLE   0x2000u
#define PAD_CROSS    0x4000u
#define PAD_SQUARE   0x8000u
#define PAD_OPTIONS  0x0008u
#define PAD_TOUCHPAD 0x100000u

// OrbisPadData: buttons is the first field; struct is ~120 bytes. Over-size to be safe.
struct PadState { uint32_t buttons; uint8_t _rest[252]; };

// Run-control state, set from the controller menu. Read by Screen::render for the bar.
static int       g_tries      = 100;   // iterations per selected test
static long long g_runs_done  = 0;     // iterations completed this run
static long long g_runs_base  = 0;     // iterations from already-finished tests
static long long g_runs_total = 1;     // selected_tests * g_tries
static long long g_tick_step  = 1;     // render the bar every N iterations
static int       g_cursor     = 0;     // highlighted menu row

struct Screen {
    int handle; bool ok; int w, h; int cur; long long flipid;
    uint32_t* fb[2];
    char lines[SCR_MAXLINES][120]; int nlines;
    long long last_tick;   // g_runs_done value at last bar render (throttle)

    static int u2s(char* b, int p, int v) {   // append unsigned decimal
        char t[12]; int n = 0;
        if (v <= 0) t[n++] = '0';
        while (v > 0) { t[n++] = (char)('0' + v % 10); v /= 10; }
        while (n > 0) b[p++] = t[--n];
        return p;
    }

    bool init() {
        ok = false; cur = 0; flipid = 0; nlines = 0; last_tick = 0;
        handle = sceVideoOutOpen(0xFF, 0, 0, 0);          // ORBIS_VIDEO_USER_MAIN, BUS_MAIN
        if (handle < 0) return false;
        w = 1920; h = 1080;   // target output is always full HD
        unsigned long fbsz = (unsigned long)w * (unsigned long)h * 4ul;
        unsigned long align = 0x200000ul;
        unsigned long total = (fbsz * 2ul + align - 1) / align * align;
        long off = 0;
        if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3, &off) < 0)
            return false;
        void* base = 0;
        if (sceKernelMapDirectMemory(&base, total, 0x33, 0, off, align) < 0) return false;
        fb[0] = (uint32_t*)base;
        fb[1] = (uint32_t*)((char*)base + fbsz);
        VideoBufAttr attr; my_memset(&attr, 0, sizeof(attr));
        sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1u, 0u,
                                      (uint32_t)w, (uint32_t)h, (uint32_t)w);  // LINEAR
        void* bufs[2] = { fb[0], fb[1] };
        if (sceVideoOutRegisterBuffers(handle, 0, bufs, 2, &attr) != 0) return false;
        sceVideoOutSetFlipRate(handle, 0);
        ok = true;
        return true;
    }

    static bool is_tag(const char* s) {   // s points past leading spaces
        if (s[0] != '[') return false;
        return (s[1]=='P'&&s[2]=='A'&&s[3]=='S'&&s[4]=='S') ||
               (s[1]=='F'&&s[2]=='A'&&s[3]=='I'&&s[4]=='L') ||
               (s[1]=='H'&&s[2]=='A'&&s[3]=='N'&&s[4]=='G') ||
               (s[1]=='S'&&s[2]=='K'&&s[3]=='I'&&s[4]=='P');
    }
    void push(const char* s, int n) {
        int i = 0;
        while (i < n) {
            int j = i, t = 0; char tmp[120];
            while (j < n && s[j] != '\n') { if (t < 119) tmp[t++] = s[j]; j++; }
            tmp[t] = 0;
            if (t > 0) {
                int k = 0; while (tmp[k] == ' ') k++;          // skip leading spaces
                bool run = (t >= 9 && tmp[0]=='='&&tmp[1]=='='&&tmp[2]=='='&&tmp[3]=='='&&
                            tmp[4]=='='&&tmp[5]==' '&&tmp[6]=='R'&&tmp[7]=='U'&&tmp[8]=='N');
                if (run) nlines = 0;                           // new run -> show only its results
                if (!run && is_tag(tmp + k) && nlines > 0) {   // merge result onto test line
                    char* prev = lines[nlines-1];
                    int pl = 0; while (prev[pl]) pl++;
                    if (pl < 118) prev[pl++] = ' ';
                    for (int c = k; tmp[c] && pl < 119; c++) prev[pl++] = tmp[c];
                    prev[pl] = 0;
                } else {
                    if (nlines >= SCR_MAXLINES) {
                        for (int kk = 1; kk < SCR_MAXLINES; kk++)
                            for (int c = 0; c < 120; c++) lines[kk-1][c] = lines[kk][c];
                        nlines = SCR_MAXLINES - 1;
                    }
                    for (int c = 0; c <= t; c++) lines[nlines][c] = tmp[c];
                    nlines++;
                }
            }
            i = (j < n) ? j + 1 : j;
        }
    }

    void glyph(uint32_t* dst, int px, int py, char ch, uint32_t col, int scale) {
        if (ch < 0x20 || ch > 0x7e) ch = '?';
        const unsigned char* g = FONT[ch - 0x20];
        for (int ry = 0; ry < 8; ry++) {
            unsigned char row = g[ry];
            for (int rx = 0; rx < 8; rx++) {
                if (!(row & (1 << rx))) continue;
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++) {
                        int x = px + rx*scale + sx, y = py + ry*scale + sy;
                        if (x >= 0 && x < w && y >= 0 && y < h) dst[(long)y*w + x] = col;
                    }
            }
        }
    }

    void clear(uint32_t bg) { long n = (long)w * h; for (long i = 0; i < n; i++) fb[cur][i] = bg; }

    void text(int x, int y, const char* s, uint32_t col, int scale) {
        int gw = 8 * scale;
        for (const char* q = s; *q; q++) { glyph(fb[cur], x, y, *q, col, scale); x += gw; if (x > w - gw) break; }
    }

    void present() {
        flipid++;
        sceVideoOutSubmitFlip(handle, cur, 1, flipid);    // 1 = VSYNC
        VideoFlipStatus st;
        for (int t = 0; t < 300; t++) {                   // bounded ~30ms; no hard stall
            my_memset(&st, 0, sizeof(st));
            sceVideoOutGetFlipStatus(handle, &st);
            if (st.flipArg == flipid) break;
            sceKernelUsleep(100);
        }
        cur = 1 - cur;
    }

    // progress bar along the bottom: '#' span = completed iterations / total
    void draw_bar(int scale) {
        uint32_t* dst = fb[cur];
        int gw = 8 * scale, rowh = 10 * scale;
        int barmax = (w - 12) / gw;                 // '#' that fit across full HD
        if (barmax < 1) barmax = 1;
        long long rt = g_runs_total > 0 ? g_runs_total : 1;
        int pct    = (int)(g_runs_done * 100 / rt);
        int filled = (int)(g_runs_done * barmax / rt);
        if (filled > barmax) filled = barmax;
        char lbl[80]; int lp = 0;
        const char* pre = "Progress ";
        while (*pre) lbl[lp++] = *pre++;
        lp = u2s(lbl, lp, pct); lbl[lp++] = '%'; lbl[lp++] = ' '; lbl[lp++] = '(';
        lp = u2s(lbl, lp, (int)g_runs_done); lbl[lp++] = '/'; lp = u2s(lbl, lp, (int)g_runs_total);
        lbl[lp++] = ' '; lbl[lp++] = 'r'; lbl[lp++] = 'u'; lbl[lp++] = 'n'; lbl[lp++] = 's'; lbl[lp++] = ')';
        lbl[lp] = 0;
        text(6, h - 2 * rowh - 4, lbl, 0x80E0E0E0u, scale);
        int bx = 6, by = h - rowh - 4;
        for (int i = 0; i < barmax; i++) {
            char ch = (i < filled) ? '#' : '-';
            uint32_t bc = (i < filled) ? 0x8040E060u : 0x80383840u;
            glyph(dst, bx, by, ch, bc, scale); bx += gw;
        }
    }

    void render() {
        if (!ok) return;
        clear(0x80101018u);
        uint32_t* dst = fb[cur];
        int scale = 2;
        if (nlines * (10*scale) > h - 8) scale = 1;
        int gw = 8*scale, rowh = 10*scale;
        int maxrows = (h - 8) / rowh;
        int first = (nlines > maxrows) ? nlines - maxrows : 0;
        int y = 4;
        for (int li = first; li < nlines; li++) {
            const char* s = lines[li];
            uint32_t col = 0x80E0E0E0u;
            for (const char* q = s; *q; q++) {
                if (q[0]=='P'&&q[1]=='A'&&q[2]=='S'&&q[3]=='S') { col = 0x8040E060u; break; }
                if ((q[0]=='F'&&q[1]=='A'&&q[2]=='I'&&q[3]=='L') ||
                    (q[0]=='H'&&q[1]=='A'&&q[2]=='N'&&q[3]=='G')) { col = 0x80E05050u; break; }
            }
            int x = 6;
            for (const char* q = s; *q; q++) { glyph(dst, x, y, *q, col, scale); x += gw; if (x > w - gw) break; }
            y += rowh;
        }
        draw_bar(scale);
        present();
    }

    // throttled bar refresh during a long test (called every iteration)
    void tick() {
        if (!ok) return;
        if (g_runs_done - last_tick < g_tick_step) return;
        last_tick = g_runs_done;
        render();
    }
};
static Screen g_screen;
static void scr_log(const char* buf, int n) { if (g_screen.ok) { g_screen.push(buf, n); g_screen.render(); } }

// ---- Bulletproof logging --------------------------------------------------
// shadps4 buffers file writes and only commits on close(); sceKernelFsync does
// not force a host-disk flush. A crash on the GpuCommandProcessor thread aborts
// the process with our file still open -> all buffered data lost. So we
// open+write+close on EVERY line: each line is committed before the next GPU op
// that might crash the emulator. Only %d, %s, %llX are used.
//   flags: 0x601 = O_WRONLY|O_CREAT|O_TRUNC (first open, clears old file)
//          0x209 = O_WRONLY|O_CREAT|O_APPEND (every subsequent line)
static const char* g_log_path = "/data/results.txt";

static void log_init() {
    int fd = sceKernelOpen(g_log_path, 0x0601, 0777);   // truncate-create
    if (fd < 0) { g_log_path = "/temp0/results.txt"; fd = sceKernelOpen(g_log_path, 0x0601, 0777); }
    if (fd >= 0) sceKernelClose(fd);
}

static int ap_str(char* b, int p, const char* s) { while (*s) b[p++] = *s++; return p; }
static int ap_dec(char* b, int p, int v) {
    if (v < 0) { b[p++] = '-'; v = -v; }
    if (v == 0) { b[p++] = '0'; return p; }
    char t[12]; int n = 0; while (v > 0) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[p++] = t[--n]; return p;
}
static int ap_hex64(char* b, int p, unsigned long long v) {
    char t[16]; int n = 0;
    if (v == 0) { b[p++] = '0'; return p; }
    while (v) { int d = (int)(v & 0xF); t[n++] = d < 10 ? (char)('0'+d) : (char)('A'+d-10); v >>= 4; }
    while (n) b[p++] = t[--n]; return p;
}

static void logf(const char* fmt, ...) {
    char buf[512]; int p = 0;
    __builtin_va_list ap; __builtin_va_start(ap, fmt);
    for (const char* f = fmt; *f && p < 500; f++) {
        if (*f != '%') { buf[p++] = *f; continue; }
        f++;
        if (*f == 'd') p = ap_dec(buf, p, __builtin_va_arg(ap, int));
        else if (*f == 's') p = ap_str(buf, p, __builtin_va_arg(ap, const char*));
        else if (*f == 'l') { while (*f == 'l') f++; /* %llX/%llx */
                              p = ap_hex64(buf, p, __builtin_va_arg(ap, unsigned long long)); }
        else if (*f == 'x' || *f == 'X') p = ap_hex64(buf, p, (unsigned long long)__builtin_va_arg(ap, unsigned int));
        else buf[p++] = *f;
    }
    __builtin_va_end(ap);
    sceKernelWrite(1, buf, (unsigned long)p);            // fd 1: shadPS4 console / PS4 klog
    int fd = sceKernelOpen(g_log_path, 0x0209, 0777);   // append
    if (fd >= 0) { sceKernelWrite(fd, buf, (unsigned long)p); sceKernelFsync(fd); sceKernelClose(fd);     scr_log(buf, p);
}
}

static void log_flush() {}        // each logf already commits via close()
static void log_close() {}


// ============================================================================
// PM4 Packet Types and Opcodes for AMD GCN (PS4 Liverpool GPU)
// ============================================================================

// PM4 Type 3 header: [31:30]=type(3), [15:8]=opcode, [29:16]=count-1, [7:0]=predicate
static inline uint32_t PM4_HDR(uint32_t opcode, uint32_t count) {
    // type3 = 3 << 30, count is N-1 dwords after header
    return (3u << 30) | ((count - 1u) << 16) | ((opcode & 0xFF) << 8);
}

// Type 2 NOP padding
static inline uint32_t PM4_TYPE2_NOP() {
    return 0x80000000u;
}

// PM4 Opcodes
enum PM4Opcode : uint32_t {
    IT_NOP                    = 0x10,
    IT_SET_BASE               = 0x11,
    IT_INDEX_BASE             = 0x12,
    IT_CLEAR_STATE            = 0x12,
    IT_INDEX_BUFFER_SIZE      = 0x13,
    IT_DISPATCH_DIRECT        = 0x15,
    IT_DISPATCH_INDIRECT      = 0x16,
    IT_SET_PREDICATION        = 0x20,
    IT_COND_EXEC              = 0x22,
    IT_INDEX_TYPE             = 0x2A,
    IT_DRAW_INDEX_2           = 0x27,
    IT_CONTEXT_CONTROL        = 0x28,
    IT_DRAW_INDEX_AUTO        = 0x2D,
    IT_NUM_INSTANCES          = 0x2F,
    IT_INDIRECT_BUFFER        = 0x3F,
    IT_STRMOUT_BUFFER_UPDATE  = 0x34,
    IT_MEM_SEMAPHORE          = 0x39,
    IT_WRITE_DATA             = 0x37,
    IT_EVENT_WRITE            = 0x46,
    IT_EVENT_WRITE_EOP        = 0x47,
    IT_EVENT_WRITE_EOS        = 0x48,
    IT_RELEASE_MEM            = 0x49,
    IT_DMA_DATA               = 0x50,
    IT_ACQUIRE_MEM            = 0x58,
    IT_REWIND                 = 0x59,
    IT_SET_CONFIG_REG         = 0x68,
    IT_SET_CONTEXT_REG        = 0x69,
    IT_SET_SH_REG             = 0x76,
    IT_SET_UCONFIG_REG        = 0x79,
    IT_SET_QUEUE_REG          = 0x78,
    IT_INCREMENT_DE_COUNTER   = 0x85,
    IT_WAIT_ON_CE_COUNTER     = 0x86,
    IT_INCREMENT_CE_COUNTER   = 0x84,
    IT_WAIT_ON_DE_COUNTER_DIFF= 0x88,
    IT_WRITE_CONST_RAM        = 0x81,
    IT_DUMP_CONST_RAM         = 0x83,
    IT_INDIRECT_BUFFER_CONST  = 0x43,
    IT_WAIT_REG_MEM           = 0x3C,
    IT_COPY_DATA              = 0x40,
    IT_PFP_SYNC_ME            = 0x42,
    IT_DISPATCH_DIRECT_MEC    = 0x15,
};

// ============================================================================
// PM4 Packet Emitters — append dwords to a command buffer
// ============================================================================

struct CmdBuffer {
    uint32_t* buf;
    uint32_t  offset;     // current write position in dwords
    uint32_t  capacity;   // max dwords

    void init(uint32_t* storage, uint32_t cap_dwords) {
        buf = storage;
        offset = 0;
        capacity = cap_dwords;
        memset(buf, 0, cap_dwords * sizeof(uint32_t));
    }

    void emit(uint32_t dw) {
        if (offset < capacity) buf[offset++] = dw;
    }

    uint32_t sizeBytes() const { return offset * sizeof(uint32_t); }
    uint32_t sizeDwords() const { return offset; }
};

// ---------------------------------------------------------------------------
// NOP with optional payload
// ---------------------------------------------------------------------------
static inline void pm4_nop(CmdBuffer& cb, uint32_t count = 1) {
    cb.emit(PM4_HDR(IT_NOP, count));
    for (uint32_t i = 0; i < count; i++) cb.emit(0); // count payload dwords
}

// ---------------------------------------------------------------------------
// WRITE_DATA — write immediate data to a memory address
//   dst_sel: 2 = memory sync, 5 = memory async
// ---------------------------------------------------------------------------
static inline void pm4_write_data(CmdBuffer& cb, void* address, const uint32_t* data, uint32_t num_dwords) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t count = 2 + num_dwords; // header body: dst_sel, addr_lo, addr_hi, data...
    cb.emit(PM4_HDR(IT_WRITE_DATA, count + 1));
    // dw1: dst_sel=5 (memory async), wr_confirm=1
    cb.emit((5u << 8) | (1u << 20));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
    for (uint32_t i = 0; i < num_dwords; i++) cb.emit(data[i]);
}

// Convenience: write a single u32
static inline void pm4_write_data_u32(CmdBuffer& cb, volatile uint32_t* address, uint32_t value) {
    pm4_write_data(cb, (void*)address, &value, 1);
}

// Convenience: write a single u64
static inline void pm4_write_data_u64(CmdBuffer& cb, volatile uint64_t* address, uint64_t value) {
    uint32_t data[2] = { (uint32_t)(value), (uint32_t)(value >> 32) };
    pm4_write_data(cb, (void*)address, data, 2);
}

// ---------------------------------------------------------------------------
// EVENT_WRITE_EOP — end-of-pipe event with fence write
//   Writes `data` to `address` after all prior work completes (on real HW).
//   data_sel: 1=32bit low, 2=64bit, 3=gpu_clock64
//   int_sel:  0=none, 2=irq_when_write_confirm
// ---------------------------------------------------------------------------
static inline void pm4_event_write_eop(CmdBuffer& cb, void* address, uint64_t data,
                                        uint32_t data_sel = 2, uint32_t int_sel = 0) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    // event_type = CACHE_FLUSH_AND_INV_TS_EVENT (0x14), event_index = 5 (EOP timestamp)
    uint32_t dw1 = (0x14u) | (5u << 8);
    uint32_t dw2 = (data_sel << 29) | (int_sel << 24);
    cb.emit(PM4_HDR(IT_EVENT_WRITE_EOP, 5));
    cb.emit(dw1);                              // event control
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));   // address lo
    cb.emit((uint32_t)(addr >> 32) | dw2);     // address hi + data/int sel
    cb.emit((uint32_t)(data));                  // data lo
    cb.emit((uint32_t)(data >> 32));            // data hi
}

// ---------------------------------------------------------------------------
// RELEASE_MEM — compute-queue fence write
// ---------------------------------------------------------------------------
static inline void pm4_release_mem(CmdBuffer& cb, void* address, uint64_t data,
                                    uint32_t data_sel = 2, uint32_t int_sel = 0) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t dw1 = (0x14u) | (5u << 8); // CACHE_FLUSH_AND_INV_TS_EVENT, event_index=5
    uint32_t dw2 = (data_sel << 29) | (int_sel << 24);
    cb.emit(PM4_HDR(IT_RELEASE_MEM, 6));
    cb.emit(dw1);
    cb.emit(dw2);
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit((uint32_t)(addr >> 32));
    cb.emit((uint32_t)(data));
    cb.emit((uint32_t)(data >> 32));
}

// ---------------------------------------------------------------------------
// WAIT_REG_MEM — poll memory until condition is met
//   func: 3=equal, 5=greater_equal, 6=greater
//   mem_space: 1=memory
// ---------------------------------------------------------------------------
static inline void pm4_wait_reg_mem(CmdBuffer& cb, volatile void* address,
                                     uint32_t ref, uint32_t mask, uint32_t func = 5) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_WAIT_REG_MEM, 6));
    // func | mem_space=1 (memory) | engine=0 (ME)
    cb.emit(func | (1u << 4));
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit((uint32_t)(addr >> 32));
    cb.emit(ref);
    cb.emit(mask);
    cb.emit(10); // poll interval
}

// ---------------------------------------------------------------------------
// MEM_SEMAPHORE — signal or wait on a memory-based semaphore
//   Signal: increments *address
//   Wait:   spins until *address > 0, then decrements
// ---------------------------------------------------------------------------
static inline void pm4_mem_semaphore_signal(CmdBuffer& cb, volatile void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_MEM_SEMAPHORE, 2));
    cb.emit((uint32_t)((addr >> 3) << 3));           // addr_lo [31:3]
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFF);  // addr_hi
    dw2 |= (6u << 29);  // sem_sel = SignalSemaphore
    dw2 |= (0u << 20);  // signal_type = Increment
    cb.emit(dw2);
}

static inline void pm4_mem_semaphore_wait(CmdBuffer& cb, volatile void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_MEM_SEMAPHORE, 2));
    cb.emit((uint32_t)((addr >> 3) << 3));
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFF);
    dw2 |= (7u << 29);  // sem_sel = WaitSemaphore
    cb.emit(dw2);
}

// ---------------------------------------------------------------------------
// DMA_DATA — GPU-side memory fill or copy
//   src_sel: 0=data (immediate), 1=memory, 3=memory_l2
//   dst_sel: 0=memory, 1=gds, 3=memory_l2
// ---------------------------------------------------------------------------
static inline void pm4_dma_data_fill(CmdBuffer& cb, void* dst, uint32_t value, uint32_t num_bytes) {
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    cb.emit(PM4_HDR(IT_DMA_DATA, 6));
    // dw1: src_sel=2 (Data/immediate), dst_sel=0 (memory), cp_sync=1
    cb.emit((2u << 29) | (0u << 20) | (1u << 31));
    cb.emit(value);                               // src_addr_lo = data value
    cb.emit(0);                                   // src_addr_hi (unused for Data)
    cb.emit((uint32_t)(daddr & 0xFFFFFFFF));      // dst_addr_lo
    cb.emit((uint32_t)(daddr >> 32));              // dst_addr_hi
    cb.emit(num_bytes);                           // command: byte count [20:0]
}

static inline void pm4_dma_data_copy(CmdBuffer& cb, void* dst, void* src, uint32_t num_bytes) {
    uint64_t saddr = (uint64_t)(uintptr_t)src;
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    cb.emit(PM4_HDR(IT_DMA_DATA, 6));
    // dw1: src_sel=3 (MemoryUsingL2), dst_sel=3 (MemoryUsingL2), cp_sync=1
    cb.emit((3u << 29) | (3u << 20) | (1u << 31));
    cb.emit((uint32_t)(saddr & 0xFFFFFFFF));   // src_addr_lo
    cb.emit((uint32_t)(saddr >> 32));           // src_addr_hi
    cb.emit((uint32_t)(daddr & 0xFFFFFFFF));    // dst_addr_lo
    cb.emit((uint32_t)(daddr >> 32));           // dst_addr_hi
    cb.emit(num_bytes);                         // command: byte count [20:0]
}

// ---------------------------------------------------------------------------
// ACQUIRE_MEM — invalidate GPU caches (should be barrier on real HW)
// ---------------------------------------------------------------------------
static inline void pm4_acquire_mem(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_ACQUIRE_MEM, 6));
    cb.emit(0x02800000); // cp_coher_cntl: TC|SH action ena
    cb.emit(0xFFFFFFFF); // cp_coher_size_lo
    cb.emit(0x000000FF); // cp_coher_size_hi
    cb.emit(0);          // cp_coher_base_lo
    cb.emit(0);          // cp_coher_base_hi
    cb.emit(10);         // poll interval
}

// ---------------------------------------------------------------------------
// PFP_SYNC_ME — PFP catches up to ME (CpSync in shadps4)
// ---------------------------------------------------------------------------
static inline void pm4_pfp_sync_me(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_PFP_SYNC_ME, 1));
    cb.emit(0);
}

// ---------------------------------------------------------------------------
// WRITE_CONST_RAM — CE writes to constant heap
// ---------------------------------------------------------------------------
static inline void pm4_write_const_ram(CmdBuffer& cb, uint32_t offset_bytes,
                                        const uint32_t* data, uint32_t num_dwords) {
    cb.emit(PM4_HDR(IT_WRITE_CONST_RAM, num_dwords + 1));
    cb.emit(offset_bytes);
    for (uint32_t i = 0; i < num_dwords; i++) cb.emit(data[i]);
}

// ---------------------------------------------------------------------------
// DUMP_CONST_RAM — CE dumps constant heap to memory
// ---------------------------------------------------------------------------
static inline void pm4_dump_const_ram(CmdBuffer& cb, void* address,
                                       uint32_t offset_bytes, uint32_t num_dwords) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_DUMP_CONST_RAM, 4));  // 4 body dwords: offset, size, addr_lo, addr_hi
    cb.emit(offset_bytes);
    cb.emit(num_dwords);
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
}

// ---------------------------------------------------------------------------
// INCREMENT_CE_COUNTER / INCREMENT_DE_COUNTER / WAIT_ON_CE_COUNTER
// ---------------------------------------------------------------------------
static inline void pm4_increment_ce_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_INCREMENT_CE_COUNTER, 1));
    cb.emit(0);
}
static inline void pm4_increment_de_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_INCREMENT_DE_COUNTER, 1));
    cb.emit(0);
}
static inline void pm4_wait_on_ce_counter(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_WAIT_ON_CE_COUNTER, 1));
    cb.emit(0);
}

// ---------------------------------------------------------------------------
// CONTEXT_CONTROL — needed at start of DCB
// ---------------------------------------------------------------------------
static inline void pm4_context_control(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_CONTEXT_CONTROL, 2));
    cb.emit(0x80000000);  // LOAD_ENABLE
    cb.emit(0x80000000);  // SHADOW_ENABLE
}

// ---------------------------------------------------------------------------
// EVENT_WRITE_EOS — end-of-shader event with fence or GDS store
//   command: 2 = SignalFence, 1 = GdsStore
// ---------------------------------------------------------------------------
static inline void pm4_event_write_eos_fence(CmdBuffer& cb, void* address, uint32_t data) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    uint32_t dw1 = (0x04u) | (4u << 8); // CS_DONE, event_index=4
    uint32_t cmd_info = ((uint32_t)(addr >> 32) & 0xFFFF) | (2u << 29); // command=SignalFence
    cb.emit(PM4_HDR(IT_EVENT_WRITE_EOS, 4));
    cb.emit(dw1);
    cb.emit((uint32_t)(addr & 0xFFFFFFFFu));
    cb.emit(cmd_info);
    cb.emit(data);
}

// ---------------------------------------------------------------------------
// INDIRECT_BUFFER — jump to secondary command buffer
// ---------------------------------------------------------------------------
static inline void pm4_indirect_buffer(CmdBuffer& cb, void* ib_address, uint32_t ib_size_dw) {
    uint64_t addr = (uint64_t)(uintptr_t)ib_address;
    cb.emit(PM4_HDR(IT_INDIRECT_BUFFER, 3));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));         // ibBaseLo (4-byte aligned)
    cb.emit((uint32_t)((addr >> 32) & 0xFFFF));     // ibBaseHi [15:0]
    // ibSize [19:0] | valid [23]. On CI (Sea Islands = Liverpool) the valid bit
    // is required or the CP rejects the IB and the ring stalls. Verified vs AMD
    // PAL gfx6 PM4CMDINDIRECTBUFFER.CI (valid:1 @ bit23) and libSceGnmDriver
    // submit path (OR ordinal4,0x800000). chain/vmid/cachePolicy = 0 (as GNM).
    cb.emit((ib_size_dw & 0xFFFFF) | (1u << 23));
}

// ---------------------------------------------------------------------------
// EVENT_WRITE — ZpassDone for occlusion queries
// ---------------------------------------------------------------------------
static inline void pm4_event_write_zpass(CmdBuffer& cb, void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    // event_type=PixelPipeStatDump(0x16), event_index=ZpassDone(1)
    cb.emit(PM4_HDR(IT_EVENT_WRITE, 3));
    cb.emit((0x16u) | (1u << 8));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));
    cb.emit((uint32_t)(addr >> 32));
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

// g_tries iterations per test; progress counts every iteration toward g_runs_total.

#define TEST_BEGIN(name) \
    do { \
        tests_run++; \
        logf("\n[TEST %d] %s\n", tests_run, name); \
        log_flush(); \
        bool _pass = true; \
        for (int _rep = 0; _rep < g_tries; _rep++) { \
            g_gpu_pool_next = 0;  /* reclaim pool each iteration */ \
            g_runs_done = g_runs_base + _rep + 1; g_screen.tick();

#define TEST_CHECK(cond, msg) \
            if (!(cond)) { \
                logf("  [FAIL] %s (iter %d/%d)\n", msg, _rep + 1, g_tries); \
                _pass = false; log_flush(); break; \
            }

#define TEST_PASS() \
        } \
        g_runs_base += g_tries; g_runs_done = g_runs_base; \
        if (_pass) { logf("  [PASS]\n"); tests_passed++; } \
        else { tests_failed++; } \
        log_flush(); \
    } while (0)

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
        memset(ring, 0, 0x10000);   // ASC thread reads ring on map; never leave garbage
        *read_ptr = 0; write_off = 0;
        vqid = sceGnmMapComputeQueue(pipe, queue, (uintptr_t)ring, 0x4000, read_ptr);
        return vqid > 0;
    }
    CmdBuffer begin() {
        // Queue is fully drained between submits (each test waits/sleeps), so
        // wrapping the ring write position to 0 when it nears the end is safe
        // and prevents (0x4000 - write_off) from underflowing -> OOB ring ptr.
        if (write_off + 0x400 > 0x4000) write_off = 0;
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
    submit_and_wait(d1, c1.sizeBytes()); if (!wait_fence(f1)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }

    uint32_t* d2 = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer c2; c2.init(d2, 0x4000);
    pm4_context_control(c2);
    pm4_acquire_mem(c2);
    pm4_dma_data_copy(c2, (void*)dst, (void*)src, 4);
    pm4_event_write_eop(c2, (void*)f2, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(d2, c2.sizeBytes()); if (!wait_fence(f2)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    // No CE/DE counter handshake: those counters are global/persistent and GNM
    // has already advanced de_count, so WAIT_ON_CE_COUNTER deadlocks on real HW.
    // The CE runs the CCB sequentially; the DCB EOP fence + sceGnmSubmitDone()
    // guarantee both engines are idle before the readback.
    uint32_t* dcb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer dcb; dcb.init(dcb_mem, 0x4000);
    pm4_context_control(dcb);
    pm4_event_write_eop(dcb, (void*)fence, 1, 2, 0);
    pm4_nop(dcb);
    submit_and_wait(dcb_mem, dcb.sizeBytes(), ccb_mem, ccb.sizeBytes());
    if (!wait_fence(fence)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    // No CE/DE counter handshake (see test_06): deadlocks on real HW. CE runs
    // the CCB sequentially; EOP fence + sceGnmSubmitDone() provide completion.
    uint32_t* dcb_mem = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer dcb; dcb.init(dcb_mem, 0x4000);
    pm4_context_control(dcb);
    pm4_event_write_eop(dcb, (void*)f, 1, 2, 0);
    pm4_nop(dcb);
    submit_and_wait(dcb_mem, dcb.sizeBytes(), ccb_mem, ccb.sizeBytes());
    if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    g_cq.submit(acb); sceKernelUsleep(10000); if (!wait_fence(f1)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }

    uint32_t* dcb = (uint32_t*)gpu_alloc(0x10000);
    CmdBuffer cb; cb.init(dcb, 0x4000);
    pm4_context_control(cb);
    pm4_acquire_mem(cb);
    pm4_dma_data_copy(cb, (void*)dst, (void*)buf, 4);
    pm4_event_write_eop(cb, (void*)f2, 1, 2, 0);
    pm4_nop(cb);
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f2)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
    TEST_CHECK(*sem == 5, "Signal count wrong");
    *f = 0;
    CmdBuffer c2; c2.init(dcb, 0x4000);
    pm4_context_control(c2);
    for (int i = 0; i < 5; i++) pm4_mem_semaphore_wait(c2, (void*)sem);
    pm4_event_write_eop(c2, (void*)f, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(dcb, c2.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
    TEST_CHECK(*sem == 32, "32x signal count wrong");
    *f = 0;
    CmdBuffer c2; c2.init(dcb, 0x4000);
    pm4_context_control(c2);
    for (int i = 0; i < 32; i++) pm4_mem_semaphore_wait(c2, (void*)sem);
    pm4_event_write_eop(c2, (void*)f, 1, 2, 0);
    pm4_nop(c2);
    submit_and_wait(dcb, c2.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
    TEST_CHECK(*sem == 0, "WAW-1c: 32x signal/wait imbalanced");
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
    TEST_CHECK(*d == 0x22222222, "WAW-4: DMA should overwrite WriteData");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 8: COND_EXEC (RAW-9)
// ============================================================================

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
    submit_and_wait(dcb, cb.sizeBytes()); if (!wait_fence(f)) { logf("  [HANG] fence timeout (iter %d/%d)\n", _rep + 1, g_tries); _pass = false; log_flush(); break; }
    TEST_CHECK(*r == 0x1B1B1B1B, "IB: WriteData not executed");
    TEST_PASS();
}

// ============================================================================
//  CATEGORY 10: SUBMIT STRESS (WAR-1)
// ============================================================================

static void test_29_rapid_submit() {
    TEST_BEGIN("WAR-1: 50 rapid submit/done cycles");
    const int N = 50;
    const uint32_t SLOT_DW = 0x40;  // 64-dword command-buffer slot per submit
    volatile uint32_t* fences = (volatile uint32_t*)gpu_alloc(0x10000);
    uint32_t* dcb_base = (uint32_t*)gpu_alloc(0x10000);
    TEST_CHECK(fences && dcb_base, "alloc"); memset((void*)fences, 0, N * 4);
    // sceGnmSubmitCommandBuffers/SubmitDone do not wait for GPU completion, so
    // each submit uses its own buffer storage. Reusing a single buffer lets the
    // CPU overwrite packets the GPU is still fetching (WAR hazard -> corruption).
    for (int i = 0; i < N; i++) {
        uint32_t* dcb = dcb_base + (uint32_t)i * SLOT_DW;
        CmdBuffer cb; cb.init(dcb, SLOT_DW);
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
    const int N = 100;
    const uint32_t SLOT_DW = 0x40;  // 64-dword command-buffer slot per submit
    volatile uint32_t* val = (volatile uint32_t*)gpu_alloc(0x10000);
    uint32_t* dcb_base = (uint32_t*)gpu_alloc(0x10000);
    TEST_CHECK(val && dcb_base, "alloc"); *val = 0;
    // Own storage per submit (see test_29): no CPU-vs-GPU buffer-reuse hazard.
    for (int i = 1; i <= N; i++) {
        uint32_t* dcb = dcb_base + (uint32_t)(i - 1) * SLOT_DW;
        CmdBuffer cb; cb.init(dcb, SLOT_DW);
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
//  TEST REGISTRY + CONTROLLER MENU
// ============================================================================

struct TestEntry { void (*fn)(); const char* name; bool compute; bool sel; };

static TestEntry g_tests[] = {
    { test_01_eop_fence_timing,         "RAW-1  EOP fence before DMA",        false, true },
    { test_03_eop_pipeline_depth,       "RAW-1b 8 sequential EOP fences",     false, true },
    { test_04_acquire_mem,              "WAW-5  AcquireMem cache invalidate",  false, true },
    { test_05_cp_sync_width,            "WAW-6  PfpSyncMe barrier width",      false, true },
    { test_06_ce_dump_const_ram,        "RAW-8  CE DumpConstRam dirty track",  false, true },
    { test_07_ce_stress,                "RAW-8b CE heap 8x write+dump",        false, true },
    { test_09_mem_semaphore_basic,      "WAW-1  MemSemaphore 5x sig+wait",     false, true },
    { test_11_mem_semaphore_stress,     "WAW-1c MemSemaphore 32x sig+wait",    false, true },
    { test_14_wrm_equal,                "WaitRegMem Equal",                    false, true },
    { test_15_wrm_gt,                   "WaitRegMem GreaterThan",              false, true },
    { test_16_wrm_lt,                   "WaitRegMem LessThan",                 false, true },
    { test_17_wrm_masked,               "WaitRegMem Masked Equal",             false, true },
    { test_18_wrm_gte,                  "WaitRegMem GreaterEqual",             false, true },
    { test_19_wrm_not_equal,            "WaitRegMem NotEqual",                 false, true },
    { test_20_dma_sequential,           "DMA Sequential fills",                false, true },
    { test_21_dma_copy_chain,           "DMA Copy chain A->B->C",              false, true },
    { test_22_dma_large_block,          "DMA 4KB fill integrity",              false, true },
    { test_23_write_data_u32,           "WriteData u32 after EOP",             false, true },
    { test_24_write_data_u64,           "WriteData u64 visibility",            false, true },
    { test_25_write_data_then_dma,      "WAW-4  WriteData then DMA fill",      false, true },
    { test_28_indirect_buffer,          "IndirectBuffer secondary cmd",        false, true },
    { test_29_rapid_submit,             "WAR-1  50 rapid submit/done",         false, true },
    { test_30_rapid_submit_same_addr,   "WAR-1b 100 submits same addr",        false, true },
    { test_02_release_mem_timing,       "RAW-2  Compute ReleaseMem fence",     true,  true },
    { test_08_acquire_mem_cross_engine, "WAW-5b AcquireMem compute->GFX",      true,  true },
    { test_10_mem_semaphore_cross_queue,"WAW-1b Cross-queue MemSemaphore",     true,  true },
};
static const int g_ntests = (int)(sizeof(g_tests) / sizeof(g_tests[0]));

static int g_pad = -1;

static bool pad_setup() {
    sceUserServiceInitialize(nullptr);
    int uid = -1; sceUserServiceGetInitialUser(&uid);
    scePadInit();
    g_pad = scePadOpen(uid, 0 /* STANDARD */, 0, nullptr);
    return g_pad >= 0;
}

static uint32_t pad_buttons() {
    if (g_pad < 0) return 0;
    PadState pd; my_memset(&pd, 0, sizeof(pd));
    scePadReadState(g_pad, &pd);
    return pd.buttons;
}

static void draw_menu() {
    if (!g_screen.ok) return;
    g_screen.clear(0x80101018u);
    int scale = 2, rowh = 10 * scale;
    int sel = 0; for (int i = 0; i < g_ntests; i++) if (g_tests[i].sel) sel++;
    g_screen.text(6, 4, "PS4 RACE SUITE  --  SELECT TESTS", 0x80FFFFFFu, scale);
    char st[96]; int p = 0;
    const char* a = "Tries "; while (*a) st[p++] = *a++;
    p = Screen::u2s(st, p, g_tries);
    const char* b = "   Selected "; while (*b) st[p++] = *b++;
    p = Screen::u2s(st, p, sel); st[p++] = '/'; p = Screen::u2s(st, p, g_ntests);
    const char* c = "   Total "; while (*c) st[p++] = *c++;
    p = Screen::u2s(st, p, sel * g_tries);
    const char* d = " runs"; while (*d) st[p++] = *d++; st[p] = 0;
    g_screen.text(6, 4 + rowh, st, 0x8080D0FFu, scale);
    int y0 = 4 + rowh * 2 + 4;
    for (int i = 0; i < g_ntests; i++) {
        char row[80]; int rp = 0;
        row[rp++] = (i == g_cursor) ? '>' : ' '; row[rp++] = ' ';
        row[rp++] = '['; row[rp++] = g_tests[i].sel ? 'x' : ' '; row[rp++] = ']'; row[rp++] = ' ';
        if (i + 1 < 10) row[rp++] = '0';
        rp = Screen::u2s(row, rp, i + 1); row[rp++] = ' ';
        const char* nm = g_tests[i].name; while (*nm && rp < 78) row[rp++] = *nm++;
        row[rp] = 0;
        uint32_t col = g_tests[i].sel ? 0x8080FF80u : 0x80707078u;
        if (i == g_cursor) col = g_tests[i].sel ? 0x80B0FFB0u : 0x80E0E0E0u;
        g_screen.text(6, y0 + i * rowh, row, col, scale);
    }
    g_screen.text(6, g_screen.h - rowh * 3 - 4,
                  "L1 +10  R1 +100  L2 +1000  R2 +10000  O reset", 0x80E0E0E0u, scale);
    g_screen.text(6, g_screen.h - rowh * 2 - 4,
                  "D-pad move  X toggle  Triangle all/none  OPTIONS run  Touchpad quit",
                  0x80E0E0E0u, scale);
    g_screen.present();
}

static void quit_app() {
    if (g_cq.vqid > 0) g_cq.destroy();
    log_close();
    sceSystemServiceLoadExec("EXIT", nullptr);
}

static void menu_loop() {
    uint32_t prev = pad_buttons();
    int rep = 0;
    draw_menu();
    while (true) {
        uint32_t bn = pad_buttons();
        uint32_t edge = bn & ~prev;
        bool dirty = false;
        int move = 0;
        if (edge & PAD_UP) move = -1;
        else if (edge & PAD_DOWN) move = +1;
        else if (bn & PAD_UP)   { if (++rep > 8 && rep % 3 == 0) move = -1; }
        else if (bn & PAD_DOWN) { if (++rep > 8 && rep % 3 == 0) move = +1; }
        else rep = 0;
        if (move) { g_cursor = (g_cursor + move + g_ntests) % g_ntests; dirty = true; }
        if (edge & PAD_L1) { g_tries += 10;    dirty = true; }
        if (edge & PAD_R1) { g_tries += 100;   dirty = true; }
        if (edge & PAD_L2) { g_tries += 1000;  dirty = true; }
        if (edge & PAD_R2) { g_tries += 10000; dirty = true; }
        if (edge & PAD_CIRCLE) { g_tries = 1;  dirty = true; }
        if (g_tries < 1) g_tries = 1;
        if (g_tries > 1000000) g_tries = 1000000;
        if (edge & PAD_CROSS) { g_tests[g_cursor].sel = !g_tests[g_cursor].sel; dirty = true; }
        if (edge & PAD_TRIANGLE) {
            bool all = true; for (int i = 0; i < g_ntests; i++) if (!g_tests[i].sel) all = false;
            for (int i = 0; i < g_ntests; i++) g_tests[i].sel = !all;
            dirty = true;
        }
        if (edge & PAD_TOUCHPAD) quit_app();
        if (edge & PAD_OPTIONS) {
            int s = 0; for (int i = 0; i < g_ntests; i++) if (g_tests[i].sel) s++;
            if (s > 0) break;
        }
        prev = bn;
        if (dirty) draw_menu();
        sceKernelUsleep(16000);
    }
}

static void run_selected() {
    int sel = 0; bool need_compute = false;
    for (int i = 0; i < g_ntests; i++)
        if (g_tests[i].sel) { sel++; if (g_tests[i].compute) need_compute = true; }
    g_runs_total = (long long)sel * g_tries;
    g_runs_base = 0; g_runs_done = 0; g_screen.last_tick = 0;
    g_tick_step = g_runs_total / 200; if (g_tick_step < 1) g_tick_step = 1;
    tests_run = 0; tests_passed = 0; tests_failed = 0; tests_skipped = 0;
    g_screen.nlines = 0;
    g_cq.write_off = 0;
    sceKernelUsleep(200000);
    gpu_pool_reset();

    logf("========================================================\n");
    logf(" RACE SUITE  |  %d tests x %d tries = %d runs\n", sel, g_tries, (int)g_runs_total);
    logf("========================================================\n");

    // Graphics-ring tests first so their results commit before any compute map.
    for (int i = 0; i < g_ntests; i++)
        if (g_tests[i].sel && !g_tests[i].compute) g_tests[i].fn();

    if (need_compute) {
        logf("\n[compute] mapping ASC queue (may abort shadps4)...\n");
        if (g_cq.vqid <= 0 && !g_cq.init(0, 0))
            logf("[compute] queue map failed - skipping compute tests\n");
        else
            for (int i = 0; i < g_ntests; i++)
                if (g_tests[i].sel && g_tests[i].compute) g_tests[i].fn();
    }

    logf("\n========================================================\n");
    logf(" Done: %d passed, %d failed / %d tests\n", tests_passed, tests_failed, tests_run);
    logf("========================================================\n");
    logf(" Press X for menu   Touchpad to quit\n");
}

static void wait_back() {
    uint32_t prev = pad_buttons();
    while (true) {
        uint32_t bn = pad_buttons();
        uint32_t edge = bn & ~prev;
        if (edge & PAD_CROSS) break;
        if (edge & PAD_TOUCHPAD) quit_app();
        prev = bn;
        sceKernelUsleep(16000);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    g_screen.init();   // on-screen results (video out); silent fallback if unavailable
    log_init();

    if (!gpu_pool_init()) {
        logf("FATAL: GPU pool init failed\n");
        log_close();
        return 1;
    }

    if (!pad_setup()) {
        // No controller: run every test once at the default tries, then exit.
        for (int i = 0; i < g_ntests; i++) g_tests[i].sel = true;
        run_selected();
        g_cq.destroy();
        log_close();
        sceKernelUsleep(5000000);
        sceSystemServiceLoadExec("EXIT", nullptr);
        return 0;
    }

    for (;;) {
        menu_loop();      // pick tests + tries (OPTIONS to run)
        run_selected();   // run with live progress bar
        wait_back();      // X returns to the menu
    }
    return 0;
}
