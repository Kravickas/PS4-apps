// ============================================================================
// PS4 GPU/CPU Race Condition Test Suite - FULL EDITION (single-file)
// 30 tests covering RAW/WAR/WAW hazards plus predication, EOS, and cross-engine
// sync. Most pass on real PS4; failures on shadps4 pinpoint emulator bugs.
// Tests 26/27 (predication, EOS PS_DONE) and 29/30 (cross-engine/cross-queue)
// currently fail on real hardware too — the packets are spec-correct (verified
// vs PAL gfx6), so those are recorded findings, not encoding bugs. Results ->
// /data/results.txt.
//
// Fixes vs multi-file version:
//   * Durable logging: fsync after EVERY write, so a crash never loses results.
//   * Output path /data/ first (writable, reliably visible) then /temp0/.
//   * GFX-ring tests run before the compute queue is mapped, so their results
//     commit even if the ASC queue map aborts under shadps4.
//   * ComputeQueue maps the ASC queue once and keeps the ring write pointer
//     monotonic, NOP-padding the tail on wrap so a re-run never resubmits a
//     write pointer behind the CP read pointer.
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
    int32_t sceVideoOutGetBufferLabelAddress(int32_t handle, uintptr_t* label_addr);
    int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, uint32_t* dcb_addrs[],
                                             uint32_t* dcb_sizes, uint32_t* ccb_addrs[],
                                             uint32_t* ccb_sizes, uint32_t vo_handle,
                                             uint32_t buf_idx, uint32_t flip_mode,
                                             int64_t flip_arg);
    uint64_t sceKernelGetProcessTime(void);
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
static bool      g_flip_hold  = false; // set by the flip probes: buffer log, don't present

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
        if (!ok || g_flip_hold) return;
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
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFFFF);  // addr_hi [15:0] (CI/VI)
    dw2 |= (6u << 29);  // sem_sel = SignalSemaphore
    dw2 |= (0u << 20);  // signal_type = Increment
    cb.emit(dw2);
}

static inline void pm4_mem_semaphore_wait(CmdBuffer& cb, volatile void* address) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_MEM_SEMAPHORE, 2));
    cb.emit((uint32_t)((addr >> 3) << 3));
    uint32_t dw2 = ((uint32_t)(addr >> 32) & 0xFFFF);  // addr_hi [15:0] (CI/VI)
    dw2 |= (7u << 29);  // sem_sel = WaitSemaphore
    cb.emit(dw2);
}

// ---------------------------------------------------------------------------
// DMA_DATA — GPU-side memory fill or copy
//   src_sel: 0=memory, 1=gds, 2=data (immediate), 3=memory_l2
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
// Predicated CP-DMA fill — same as pm4_dma_data_fill but with the type-3
// predicate bit set, so SET_PREDICATION gates whether the CP runs it. PAL sets
// this same bit on DMA_DATA (gfx6CmdUtil.cpp BuildDmaData uses dmaData.predicate).
// ---------------------------------------------------------------------------
static inline void pm4_dma_data_fill_pred(CmdBuffer& cb, void* dst, uint32_t value,
                                          uint32_t num_bytes) {
    uint64_t daddr = (uint64_t)(uintptr_t)dst;
    cb.emit(PM4_HDR(IT_DMA_DATA, 6) | 1u);        // bit0 = predicate
    cb.emit((2u << 29) | (0u << 20) | (1u << 31));
    cb.emit(value);
    cb.emit(0);
    cb.emit((uint32_t)(daddr & 0xFFFFFFFF));
    cb.emit((uint32_t)(daddr >> 32));
    cb.emit(num_bytes);
}

// ---------------------------------------------------------------------------
// SET_PREDICATION — gate subsequent predicated packets on a predicate source.
// pred_op: 0=CLEAR(disable), 3=MEM(value at addr). boolean = DrawIf polarity;
// continueBit=0 replaces active predication. Fields per PM4CMDSETPREDICATION:
// startAddrHi[7:0], predicationBoolean[8], predOp[18:16]. PAL requires the
// predicate address to be 16-byte aligned (BuildSetPredication asserts addr&0xF==0).
// ---------------------------------------------------------------------------
static const uint32_t SET_PRED_CLEAR = 0;
static const uint32_t SET_PRED_MEM   = 3;
static inline void pm4_set_predication(CmdBuffer& cb, void* pred_addr, uint32_t pred_op,
                                       uint32_t boolean) {
    uint64_t addr = (uint64_t)(uintptr_t)pred_addr;
    cb.emit(PM4_HDR(IT_SET_PREDICATION, 2));
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));                  // startAddressLo
    cb.emit(((uint32_t)(addr >> 32) & 0xFF)                  // startAddrHi[7:0]
            | ((boolean & 1u) << 8)                          // predicationBoolean[8]
            | ((pred_op & 7u) << 16));                       // predOp[18:16]
}

// ---------------------------------------------------------------------------
// EVENT_WRITE_EOS, PS_DONE — graphics-ring end-of-shader fence. PS_DONE=0x30 is
// the pixel-shader-done event (CS_DONE=0x2f is the compute-ring variant; using it
// on the GFX ring is invalid). eventIndex CSDONE_PSDONE=6 [11:8], command
// STORE_32BIT_DATA_TO_MEMORY=2 [31:29]. Body = 4 dwords.
// ---------------------------------------------------------------------------
static inline void pm4_event_write_eos_psdone(CmdBuffer& cb, void* address, uint32_t data) {
    uint64_t addr = (uint64_t)(uintptr_t)address;
    cb.emit(PM4_HDR(IT_EVENT_WRITE_EOS, 4));
    cb.emit(0x30u | (6u << 8));                              // PS_DONE, eventIndex=6
    cb.emit((uint32_t)(addr & 0xFFFFFFFF));                 // addressLo
    cb.emit(((uint32_t)(addr >> 32) & 0xFFFF) | (2u << 29)); // addrHi[15:0], command=STORE_32BIT
    cb.emit(data);
}

// ---------------------------------------------------------------------------
// ACQUIRE_MEM — invalidate GPU caches (should be barrier on real HW)
// ---------------------------------------------------------------------------
static inline void pm4_acquire_mem(CmdBuffer& cb) {
    cb.emit(PM4_HDR(IT_ACQUIRE_MEM, 6));
    cb.emit(0x02800000); // cp_coher_cntl: TC_ACTION_ENA (b23) | CB_ACTION_ENA (b25)
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
        // The CP processes the ring from its read pointer up to the submitted
        // write pointer, so on wrap it walks the tail (read ptr..ring end)
        // before returning to 0. Fill that tail with type-2 NOPs so the CP
        // skips valid padding instead of stale ring memory, then wrap.
        if (write_off + 0x400 > 0x4000) {
            for (uint32_t i = write_off; i < 0x4000; i++) ring[i] = 0x80000000u;
            write_off = 0;
        }
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
    void destroy() { if (vqid > 0) { sceGnmUnmapComputeQueue((uint32_t)vqid); vqid = -1; } }
};

static ComputeQueue g_cq;

// ============================================================================
//  DRAW PIPELINE (predication + EOS tests)
//  A fullscreen-quad draw: standard MVP vertex shader fed an identity MVP plus
//  6 NDC verts (passes straight through), and a solid-color pixel shader. This
//  gives the pixel pipe real work so EVENT_WRITE_EOS PS_DONE actually fires
//  (test 27) and gives SET_PREDICATION a draw to gate (test 26). Shaders and
//  register state are ported from the CUBETST00 background pass. The VS
//  PGM_RSRC1 VGPR count is corrected: the cube set 4 (20 VGPRs) but the VS
//  exports up to v45 (needs >=46), so 0x0B selects 48 VGPRs.
// ============================================================================

static const uint32_t CTX_DEPTH_RENDER_CONTROL  = 0x000;
static const uint32_t CTX_DEPTH_VIEW            = 0x002;
static const uint32_t CTX_DEPTH_RENDER_OVERRIDE = 0x003;
static const uint32_t CTX_SCREEN_SCISSOR        = 0x00C;
static const uint32_t CTX_DB_Z_INFO             = 0x010;
static const uint32_t CTX_DB_STENCIL_INFO       = 0x011;
static const uint32_t CTX_DB_Z_READ_BASE        = 0x012;
static const uint32_t CTX_DB_DEPTH_SIZE         = 0x016;
static const uint32_t CTX_DB_DEPTH_SLICE        = 0x017;
static const uint32_t CTX_WINDOW_SCISSOR        = 0x081;
static const uint32_t CTX_COLOR_TARGET_MASK     = 0x08E;
static const uint32_t CTX_COLOR_SHADER_MASK     = 0x08F;
static const uint32_t CTX_GENERIC_SCISSOR       = 0x090;
static const uint32_t CTX_VIEWPORT_SCISSOR0     = 0x094;
static const uint32_t CTX_INDEX_OFFSET          = 0x102;
static const uint32_t CTX_VIEWPORT0             = 0x10F;
static const uint32_t CTX_PS_INPUT_CNTL_0       = 0x191;
static const uint32_t CTX_VS_OUTPUT_CONFIG      = 0x1B1;
static const uint32_t CTX_PS_INPUT_ENA          = 0x1B3;
static const uint32_t CTX_PS_INPUT_ADDR         = 0x1B4;
static const uint32_t CTX_NUM_INTERP            = 0x1B6;
static const uint32_t CTX_SHADER_POS_FORMAT     = 0x1C3;
static const uint32_t CTX_Z_EXPORT_FORMAT       = 0x1C4;
static const uint32_t CTX_COLOR_EXPORT_FORMAT   = 0x1C5;
static const uint32_t CTX_BLEND_CONTROL0        = 0x1E0;
static const uint32_t CTX_DEPTH_CONTROL         = 0x200;
static const uint32_t CTX_COLOR_CONTROL         = 0x202;
static const uint32_t CTX_CLIPPER_CONTROL       = 0x204;
static const uint32_t CTX_POLYGON_CONTROL       = 0x205;
static const uint32_t CTX_VIEWPORT_CONTROL      = 0x206;
static const uint32_t CTX_VS_OUTPUT_CONTROL     = 0x207;
static const uint32_t CTX_MODE_CONTROL          = 0x292;
static const uint32_t CTX_INDEX_SIZE            = 0x29D;
static const uint32_t CTX_STAGE_ENABLE          = 0x2D5;
static const uint32_t CTX_AA_CONFIG             = 0x2F8;
static const uint32_t CTX_CB_COLOR0_BASE        = 0x318;

static const uint32_t SH_PS_PGM_LO              = 0x08;
static const uint32_t SH_PS_USER_DATA_0         = 0x0C;
static const uint32_t SH_VS_PGM_LO              = 0x48;
static const uint32_t SH_VS_USER_DATA_0         = 0x4C;

static const uint32_t UCFG_PRIMITIVE_TYPE       = 0x242;
static const uint32_t UCFG_NUM_INSTANCES        = 0x24D;

static const uint32_t DRAW_W = 64;
static const uint32_t DRAW_H = 64;

// VS: MVP vertex shader (CUBETST00 vs_shader_binary). Reads MVP at V#+0,
// sun at V#+64, and vert[vid] at V#+80 (stride 48: pos.xyzw, normal, clip.xy).
static const uint32_t vs_draw_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000,
    0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085,
    0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601,
    0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C,
    0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13,
    0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E,
    0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80000CF,
    0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF8000A1F, 0x06040302, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0x47505508, 0xAABBEE02, 0x00000000,
};

// PS: solid-color (CUBETST00 ps_shadow_clear_binary) — outputs (1,0,0,1).
static const uint32_t ps_draw_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009,
    0x7E5002F2, 0x7E520280, 0x7E540280, 0x7E5602F2,
    0xF800180F, 0x2B2A2928,
    0xBF810000,
    0x5362724F, 0x00726468,
    0x00002400, 0x00000000, 0xDEADBEEF,
    0xCAFE0108, 0x00000000,
};

static inline void emit_f(CmdBuffer& cb, float v) {
    union { float f; uint32_t u; } c; c.f = v; cb.emit(c.u);
}

static inline void pm4_set_context_reg(CmdBuffer& cb, uint32_t off, uint32_t val) {
    cb.emit(PM4_HDR(IT_SET_CONTEXT_REG, 2)); cb.emit(off); cb.emit(val);
}
static inline void pm4_set_context_regs(CmdBuffer& cb, uint32_t off,
                                        const uint32_t* vals, uint32_t n) {
    cb.emit(PM4_HDR(IT_SET_CONTEXT_REG, n + 1)); cb.emit(off);
    for (uint32_t i = 0; i < n; i++) cb.emit(vals[i]);
}
static inline void pm4_set_sh_regs(CmdBuffer& cb, uint32_t off,
                                   const uint32_t* vals, uint32_t n) {
    cb.emit(PM4_HDR(IT_SET_SH_REG, n + 1)); cb.emit(off);
    for (uint32_t i = 0; i < n; i++) cb.emit(vals[i]);
}
static inline void pm4_set_uconfig_reg(CmdBuffer& cb, uint32_t off, uint32_t val) {
    cb.emit(PM4_HDR(IT_SET_UCONFIG_REG, 2)); cb.emit(off); cb.emit(val);
}
static inline void pm4_draw_index_auto(CmdBuffer& cb, uint32_t count) {
    cb.emit(PM4_HDR(IT_DRAW_INDEX_AUTO, 2)); cb.emit(count); cb.emit(2);
}
// Predicated draw: predicate bit (bit 0) set in the type-3 header so an active
// SET_PREDICATION gates this draw.
static inline void pm4_draw_index_auto_pred(CmdBuffer& cb, uint32_t count) {
    cb.emit(PM4_HDR(IT_DRAW_INDEX_AUTO, 2) | 1u); cb.emit(count); cb.emit(2);
}

// Persistent draw resources (direct memory — survives the per-iteration pool reset).
struct DrawRes {
    void*     color;
    void*     depth;
    uint32_t* vb;
    uint32_t* desc;
    void*     vs_gpu;   // shader code must live in GPU-mapped memory, not the ELF segment
    void*     ps_gpu;
    uint32_t  vsharp[4];
    bool      ready;

    static void* dmem(size_t size) {
        long phys = 0; void* ptr = nullptr;
        if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, 0x10000, 3, &phys) < 0)
            return nullptr;
        if (sceKernelMapDirectMemory(&ptr, size, 0x33, 0, phys, 0x10000) < 0)
            return nullptr;
        return ptr;
    }

    bool init() {
        color = dmem(0x100000);
        depth = dmem(0x100000);
        vb    = (uint32_t*)dmem(0x10000);
        desc  = (uint32_t*)dmem(0x10000);
        vs_gpu = dmem(0x10000);
        ps_gpu = dmem(0x10000);
        if (!color || !depth || !vb || !desc || !vs_gpu || !ps_gpu) return false;

        // Shader code must be fetched from GPU-mapped memory; copy the binaries
        // out of the ELF data segment into direct memory (256-byte aligned base).
        for (unsigned i = 0; i < sizeof(vs_draw_binary)/4; i++)
            ((uint32_t*)vs_gpu)[i] = vs_draw_binary[i];
        for (unsigned i = 0; i < sizeof(ps_draw_binary)/4; i++)
            ((uint32_t*)ps_gpu)[i] = ps_draw_binary[i];

        float* f = (float*)vb;
        for (int i = 0; i < 16; i++) f[i] = (i % 5 == 0) ? 1.0f : 0.0f;  // identity MVP @ V#+0
        f[16] = 0.20f; f[17] = 0.50f; f[18] = 0.67f; f[19] = 0.0f;       // sun @ V#+64

        float* bg = (float*)((char*)vb + 80);                            // verts @ V#+80
        static const float bp[6][4] = {
            {-1,-1,.999f,1}, {1,-1,.999f,1}, {1,1,.999f,1},
            {-1,-1,.999f,1}, {1,1,.999f,1}, {-1,1,.999f,1},
        };
        for (int v = 0; v < 6; v++) {
            int i = v * 12;
            bg[i]   = bp[v][0]; bg[i+1] = bp[v][1]; bg[i+2] = bp[v][2]; bg[i+3] = bp[v][3];
            bg[i+4] = 0; bg[i+5] = 0; bg[i+6] = 0; bg[i+7] = 0;
            bg[i+8] = bp[v][0]; bg[i+9] = bp[v][1]; bg[i+10] = 0; bg[i+11] = 0;
        }

        uint64_t a = (uint64_t)(uintptr_t)vb;
        vsharp[0] = (uint32_t)a;
        vsharp[1] = (uint32_t)(a >> 32) & 0xFFFF;
        vsharp[2] = 0x10000;
        vsharp[3] = (1u<<3)|(2u<<6)|(3u<<9)|(4u<<12)|(4u<<15);

        ready = true;
        return true;
    }
};
static DrawRes g_draw;

// Emit the full graphics pipeline state and binds for the fullscreen quad.
// Does NOT issue the draw — the caller issues pm4_draw_index_auto[_pred].
static void emit_draw_state(CmdBuffer& cb) {
    const uint32_t W = DRAW_W, H = DRAW_H;

    { uint64_t a = (uint64_t)(uintptr_t)g_draw.vs_gpu;
      uint32_t r[4] = { (uint32_t)(a>>8), (uint32_t)(a>>40), 0x0Bu, (4u<<1) };
      pm4_set_sh_regs(cb, SH_VS_PGM_LO, r, 4); }

    { uint64_t a = (uint64_t)(uintptr_t)g_draw.ps_gpu;
      uint32_t r[4] = { (uint32_t)(a>>8), (uint32_t)(a>>40), 0x0Au, (2u<<1) };
      pm4_set_sh_regs(cb, SH_PS_PGM_LO, r, 4);
      uint32_t ud[2] = { (uint32_t)((uint64_t)(uintptr_t)g_draw.desc),
                         (uint32_t)((uint64_t)(uintptr_t)g_draw.desc >> 32) };
      pm4_set_sh_regs(cb, SH_PS_USER_DATA_0, ud, 2); }

    { uint32_t s[2] = { 0, (W & 0x7FFF) | ((H & 0x7FFF) << 16) };
      pm4_set_context_regs(cb, CTX_SCREEN_SCISSOR, s, 2);
      pm4_set_context_regs(cb, CTX_GENERIC_SCISSOR, s, 2);
      pm4_set_context_regs(cb, CTX_VIEWPORT_SCISSOR0, s, 2);
      s[0] = (1u << 31);
      pm4_set_context_regs(cb, CTX_WINDOW_SCISSOR, s, 2); }

    cb.emit(PM4_HDR(IT_SET_CONTEXT_REG, 7)); cb.emit(CTX_VIEWPORT0);
    emit_f(cb, (float)W * 0.5f);  emit_f(cb, (float)W * 0.5f);
    emit_f(cb, (float)H * -0.5f); emit_f(cb, (float)H * 0.5f);
    emit_f(cb, 1.0f);             emit_f(cb, 0.0f);

    pm4_set_context_reg(cb, CTX_INDEX_OFFSET, 0);

    pm4_set_context_reg(cb, CTX_DEPTH_RENDER_CONTROL, 1u);
    pm4_set_context_reg(cb, CTX_DEPTH_VIEW, 0);
    pm4_set_context_reg(cb, CTX_DEPTH_RENDER_OVERRIDE, 0);
    pm4_set_context_reg(cb, 0x00B, 0x3F800000u);
    pm4_set_context_reg(cb, CTX_DB_Z_INFO, 3u);
    pm4_set_context_reg(cb, CTX_DB_STENCIL_INFO, 0);
    { uint32_t z = (uint32_t)((uint64_t)(uintptr_t)g_draw.depth >> 8);
      uint32_t d[4] = { z, 0, z, 0 };
      pm4_set_context_regs(cb, CTX_DB_Z_READ_BASE, d, 4); }
    pm4_set_context_reg(cb, CTX_DB_DEPTH_SIZE, ((W/8)-1) | (((H/8)-1)<<11));
    pm4_set_context_reg(cb, CTX_DB_DEPTH_SLICE, (W*H/64)-1);
    pm4_set_context_reg(cb, CTX_DEPTH_CONTROL, (1u<<1)|(7u<<4));

    pm4_set_context_reg(cb, CTX_POLYGON_CONTROL, 0);

    { uint32_t c = (uint32_t)((uint64_t)(uintptr_t)g_draw.color >> 8);
      uint32_t r[14] = { c, (W/8)-1, (W*H/64)-1, 0, 0x09A8u, 0,0,0,0,0,0,0,0,0 };
      pm4_set_context_regs(cb, CTX_CB_COLOR0_BASE, r, 14);
      cb.emit(0xC0001000u); cb.emit(W | (H << 16)); }

    pm4_set_context_reg(cb, CTX_COLOR_TARGET_MASK, 0xF);
    pm4_set_context_reg(cb, CTX_COLOR_SHADER_MASK, 0xF);
    pm4_set_context_reg(cb, CTX_PS_INPUT_CNTL_0, 0);
    pm4_set_context_reg(cb, CTX_PS_INPUT_CNTL_0 + 1, 1);
    pm4_set_context_reg(cb, CTX_VS_OUTPUT_CONFIG, 1);
    pm4_set_context_reg(cb, CTX_PS_INPUT_ENA, 0x02);
    pm4_set_context_reg(cb, CTX_PS_INPUT_ADDR, 0x02);
    pm4_set_context_reg(cb, CTX_NUM_INTERP, 2);
    pm4_set_context_reg(cb, CTX_SHADER_POS_FORMAT, 4);
    pm4_set_context_reg(cb, CTX_Z_EXPORT_FORMAT, 0);
    pm4_set_context_reg(cb, CTX_COLOR_EXPORT_FORMAT, 9);
    pm4_set_context_reg(cb, CTX_COLOR_CONTROL, 0x00CC0010u);
    pm4_set_context_reg(cb, 0x203, 0);
    pm4_set_context_reg(cb, CTX_CLIPPER_CONTROL, 1u<<19);
    pm4_set_context_reg(cb, CTX_VIEWPORT_CONTROL, 0x43F);
    pm4_set_context_reg(cb, CTX_VS_OUTPUT_CONTROL, 0);
    pm4_set_context_reg(cb, CTX_MODE_CONTROL, 0);
    pm4_set_context_reg(cb, CTX_STAGE_ENABLE, 0);
    pm4_set_context_reg(cb, CTX_AA_CONFIG, 0);
    pm4_set_context_reg(cb, CTX_BLEND_CONTROL0, 0);
    pm4_set_context_reg(cb, CTX_INDEX_SIZE, 0);
    pm4_set_uconfig_reg(cb, UCFG_PRIMITIVE_TYPE, 4);
    pm4_set_uconfig_reg(cb, UCFG_NUM_INSTANCES, 1);
    pm4_set_sh_regs(cb, SH_VS_USER_DATA_0, g_draw.vsharp, 4);
}


// ============================================================================
//  CATEGORY 1: FENCE TIMING (RAW-1)
// ============================================================================

// ===========================================================================
// VO flip / IRQ path probes
//
// From libSceGnmDriver PatchFlipRequest: the reserved 64-dword NOP block at the
// tail of a dcb (header 0xc03e1000 = IT_NOP, 63 payload dwords, PrepareFlip
// marker in payload[0]) is rewritten into WRITE_DATA(vo_label[buf_idx] = 1)
// followed by NOP(PatchedFlip 0x68750776). sceGnmInsertWaitFlipDone emits
// WAIT_REG_MEM(func=equal, ref=0) on the same label, so 1 = locked, 0 = free.
//
// The dcb needs nothing else: no shaders, no draws, no register state.
// ===========================================================================

#define PREPARE_FLIP_MARKER 0x68750777u      // PM4CmdNop::PrepareFlip
#define FLIP_DCB_DWORDS     64
#define FLIP_RING_SLOTS     256              // 64 KB page / 256 B per dcb

static uintptr_t g_label_base = 0;
static uint32_t* g_flip_ring  = nullptr;
static int       g_flip_slot  = 0;

static inline volatile uint64_t* vo_label(int i) {
    return (volatile uint64_t*)(g_label_base + (uintptr_t)i * 8u);
}

// One page carved into 256-byte dcbs, cycled. gpu_alloc() hands out whole 64 KB
// pages, so a page per flip would exhaust the pool during the stress probe.
static bool flip_probe_init() {
    g_label_base = 0;
    g_flip_slot  = 0;
    if (g_screen.handle < 0) return false;
    g_flip_ring = (uint32_t*)gpu_alloc(GPU_PAGE);
    if (!g_flip_ring) return false;
    if (sceVideoOutGetBufferLabelAddress(g_screen.handle, &g_label_base) != 16) return false;
    return g_label_base != 0;
}

static int submit_eop_flip(int buf_idx, int64_t flip_arg) {
    uint32_t* dcb = g_flip_ring + (size_t)g_flip_slot * FLIP_DCB_DWORDS;
    g_flip_slot = (g_flip_slot + 1) % FLIP_RING_SLOTS;
    CmdBuffer cb; cb.init(dcb, FLIP_DCB_DWORDS);
    cb.emit(PM4_HDR(IT_NOP, 63));                       // 0xc03e1000
    cb.emit(PREPARE_FLIP_MARKER);
    for (int i = 0; i < 62; i++) cb.emit(0);
    uint32_t* dp[1] = { dcb };
    uint32_t  ds[1] = { cb.sizeBytes() };
    int rc = sceGnmSubmitAndFlipCommandBuffers(1, dp, ds, nullptr, nullptr,
                                               (uint32_t)g_screen.handle,
                                               (uint32_t)buf_idx, 1u, flip_arg);
    if (rc == 0) sceGnmSubmitDone();
    return rc;
}

// The displayed buffer holds its label at 1 until another flip replaces it, so
// "settled" is the pending count, never the labels reading zero.
static bool flip_wait_settled(uint32_t timeout_us) {
    while (timeout_us > 0) {
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_screen.handle, &st);
        if (st.numFlipPending <= 0) return true;
        sceKernelUsleep(200);
        timeout_us = (timeout_us > 200u) ? timeout_us - 200u : 0u;
    }
    return false;
}

struct FlipSample { uint32_t t, l0, l1; int32_t arg, cur, pend, gpu; };
static FlipSample g_ftrace[48];
static int        g_nftrace = 0;

static void ftrace_reset() { g_nftrace = 0; }

static void ftrace_sample(uint64_t t0) {
    VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
    sceVideoOutGetFlipStatus(g_screen.handle, &st);
    FlipSample s;
    s.t    = (uint32_t)(sceKernelGetProcessTime() - t0);
    s.l0   = (uint32_t)*vo_label(0);
    s.l1   = (uint32_t)*vo_label(1);
    s.arg  = (int32_t)st.flipArg;
    s.cur  = st.currentBuffer;
    s.pend = st.numFlipPending;
    s.gpu  = st.numGpuFlipPending;
    if (g_nftrace > 0) {
        FlipSample& q = g_ftrace[g_nftrace - 1];
        if (q.l0 == s.l0 && q.l1 == s.l1 && q.arg == s.arg && q.cur == s.cur &&
            q.pend == s.pend && q.gpu == s.gpu) return;          // changes only
    }
    if (g_nftrace < (int)(sizeof(g_ftrace) / sizeof(g_ftrace[0]))) g_ftrace[g_nftrace++] = s;
}

static void ftrace_dump() {
    for (int i = 0; i < g_nftrace; i++) {
        FlipSample& s = g_ftrace[i];
        logf("    t=%d l0=%d l1=%d arg=%d cur=%d pend=%d gpu=%d\n",
             (int)s.t, (int)s.l0, (int)s.l1, s.arg, s.cur, s.pend, s.gpu);
    }
}

// FLIP-1: lifecycle of a single EOP flip.
static void test_31_flip_label_basic() {
    TEST_BEGIN("FLIP-1 EOP flip label lifecycle");
    g_flip_hold = true;
    TEST_CHECK(flip_probe_init(), "sceVideoOutGetBufferLabelAddress failed");
    TEST_CHECK(flip_wait_settled(2000000), "prior flips never settled");
    ftrace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    ftrace_sample(t0);
    TEST_CHECK(submit_eop_flip(0, 901) == 0, "EOP flip submit failed");
    for (int i = 0; i < 10000; i++) {
        ftrace_sample(t0);
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_screen.handle, &st);
        if (st.flipArg == 901 && st.numFlipPending <= 0) break;
        sceKernelUsleep(200);
    }
    ftrace_sample(t0);
    ftrace_dump();
    logf("  [DATA] label0_after_own_flip=%d\n", (int)*vo_label(0));
    g_flip_hold = false;
    TEST_PASS();
}

// FLIP-2: the decisive probe.
// A->buf0, B->buf1, C->buf0 back to back with no WaitFlipDone, so buf0 is
// re-armed (label driven back to 1 by C) while it is still the displayed
// buffer. When B is presented buf0 stops being displayed: an unconditional
// label clear reads 0 even though C is still pending, a conditional clear
// leaves it at 1.
static void test_32_flip_label_rearm() {
    TEST_BEGIN("FLIP-2 re-armed buffer replaced");
    g_flip_hold = true;
    TEST_CHECK(flip_probe_init(), "sceVideoOutGetBufferLabelAddress failed");
    TEST_CHECK(flip_wait_settled(2000000), "prior flips never settled");
    ftrace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    ftrace_sample(t0);
    int rA = submit_eop_flip(0, 1001); ftrace_sample(t0);
    int rB = submit_eop_flip(1, 1002); ftrace_sample(t0);
    int rC = submit_eop_flip(0, 1003); ftrace_sample(t0);
    TEST_CHECK(rA == 0 && rB == 0 && rC == 0, "EOP flip submit failed");
    int l0_at_B = -1, l0_late_B = -1;
    uint64_t t_B = 0;
    for (int i = 0; i < 15000; i++) {
        ftrace_sample(t0);
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_screen.handle, &st);
        if (l0_at_B < 0 && st.flipArg == 1002) {
            l0_at_B = (int)*vo_label(0);
            t_B = sceKernelGetProcessTime();
        }
        if (l0_at_B >= 0 && l0_late_B < 0 && (sceKernelGetProcessTime() - t_B) >= 3000u)
            l0_late_B = (int)*vo_label(0);
        if (st.flipArg == 1003 && l0_late_B >= 0) break;
        sceKernelUsleep(200);
    }
    ftrace_sample(t0);
    ftrace_dump();
    logf("  [DATA] label0_at_B=%d label0_3ms_after_B=%d (1=re-arm survives, 0=cleared)\n",
         l0_at_B, l0_late_B);
    flip_wait_settled(2000000);
    g_flip_hold = false;
    TEST_PASS();
}

// FLIP-3: queue depth and the full return code. PatchFlipRequest maps
// sceVideoOutSubmitEopFlip's 0x80290012 to 0x80D11081.
static void test_33_flip_queue_full() {
    TEST_BEGIN("FLIP-3 flip queue depth / full rc");
    g_flip_hold = true;
    TEST_CHECK(flip_probe_init(), "sceVideoOutGetBufferLabelAddress failed");
    TEST_CHECK(flip_wait_settled(2000000), "prior flips never settled");
    int n_ok = 0, first_err = 0;
    for (int i = 0; i < 32; i++) {
        int rc = submit_eop_flip(i & 1, 2000 + i);
        if (rc == 0) { n_ok++; continue; }
        first_err = rc;
        break;
    }
    logf("  [DATA] accepted=%d first_err=%llX (0x80D11081 = queue full)\n",
         n_ok, (unsigned long long)(uint32_t)first_err);
    flip_wait_settled(4000000);
    g_flip_hold = false;
    TEST_PASS();
}

// FLIP-4: sustained rapid reuse. This is the shape that makes shadPS4's
// "Out of order flip IRQ" assert fire: the presenter clears a re-armed
// buffer's label between the GPU write and the flip marker. On hardware it is
// a throughput test.
static void test_34_flip_stress() {
    TEST_BEGIN("FLIP-4 sustained rapid reuse");
    g_flip_hold = true;
    TEST_CHECK(flip_probe_init(), "sceVideoOutGetBufferLabelAddress failed");
    TEST_CHECK(flip_wait_settled(2000000), "prior flips never settled");
    const int TARGET = 600;
    int sent = 0, full = 0, err = 0, last_err = 0;
    uint64_t t0 = sceKernelGetProcessTime();
    for (int i = 0; i < TARGET; i++) {
        int rc = submit_eop_flip(i & 1, 3000 + i);
        if (rc == 0) { sent++; continue; }
        if ((uint32_t)rc == 0x80D11081u) { full++; sceKernelUsleep(2000); i--; continue; }
        err++; last_err = rc; break;
    }
    uint64_t dt = sceKernelGetProcessTime() - t0;
    flip_wait_settled(5000000);
    logf("  [DATA] sent=%d requeues=%d errors=%d last_err=%llX us=%d\n",
         sent, full, err, (unsigned long long)(uint32_t)last_err, (int)dt);
    logf("  [DATA] labels l0=%d l1=%d - reached here = no flip IRQ assert\n",
         (int)*vo_label(0), (int)*vo_label(1));
    g_flip_hold = false;
    TEST_PASS();
}

struct TestEntry { void (*fn)(); const char* name; bool compute; bool sel; int tries; };

static TestEntry g_tests[] = {
    { test_31_flip_label_basic,  "FLIP-1 EOP flip label lifecycle", false, true,  1 },
    { test_32_flip_label_rearm,  "FLIP-2 re-armed buffer replaced", false, true,  5 },
    { test_33_flip_queue_full,   "FLIP-3 flip queue depth / rc",    false, true,  1 },
    { test_34_flip_stress,       "FLIP-4 sustained rapid reuse",    false, true,  3 },
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
    int scale = 2, rowh = 10 * scale, gw = 8 * scale;
    int sel = 0; long long total = 0;
    for (int i = 0; i < g_ntests; i++) if (g_tests[i].sel) { sel++; total += g_tests[i].tries; }

    // logo, top-right
    const char* logo = "shadPS4";
    int lw = 0; for (const char* q = logo; *q; q++) lw++;
    g_screen.text(g_screen.w - lw * gw - 8, 4, logo, 0x804AA0FFu, scale);

    g_screen.text(6, 4, "PS4 RACE SUITE -- SELECT TESTS", 0x80FFFFFFu, scale);

    char st[96]; int p = 0;
    const char* b = "Selected "; while (*b) st[p++] = *b++;
    p = Screen::u2s(st, p, sel); st[p++] = '/'; p = Screen::u2s(st, p, g_ntests);
    const char* c = "   Total "; while (*c) st[p++] = *c++;
    p = Screen::u2s(st, p, (int)total);
    const char* d = " runs"; while (*d) st[p++] = *d++; st[p] = 0;
    g_screen.text(6, 4 + rowh, st, 0x8080D0FFu, scale);

    int y0 = 4 + rowh * 2 + 4;
    // row 0: ALL TESTS bulk control
    {
        const char* all = "  >> ALL TESTS  (set tries for every test)";
        char arow[64]; int ap = 0;
        arow[ap++] = (g_cursor == 0) ? '>' : ' ';
        const char* s = all + 1; while (*s) arow[ap++] = *s++; arow[ap] = 0;
        g_screen.text(6, y0, arow, (g_cursor == 0) ? 0x804AA0FFu : 0x80FFFFFFu, scale);
    }
    for (int i = 0; i < g_ntests; i++) {
        bool oncur = (g_cursor == i + 1);
        char row[96]; int rp = 0;
        row[rp++] = oncur ? '>' : ' '; row[rp++] = ' ';
        row[rp++] = '['; row[rp++] = g_tests[i].sel ? 'x' : ' '; row[rp++] = ']'; row[rp++] = ' ';
        if (i + 1 < 10) row[rp++] = '0';
        rp = Screen::u2s(row, rp, i + 1); row[rp++] = ' ';
        const char* nm = g_tests[i].name; while (*nm && rp < 58) row[rp++] = *nm++;
        while (rp < 52) row[rp++] = ' ';            // align the tries column
        row[rp++] = '[';
        rp = Screen::u2s(row, rp, g_tests[i].tries);
        row[rp++] = 'x'; row[rp++] = ']';
        row[rp] = 0;
        // selected = white, unselected = dark grey
        uint32_t col = g_tests[i].sel ? 0x80FFFFFFu : 0x80606068u;
        if (oncur) col = 0x804AA0FFu;                                 // cursor line blue
        g_screen.text(6, y0 + (i + 1) * rowh, row, col, scale);
    }
    g_screen.text(6, g_screen.h - rowh * 3 - 4,
                  "D-pad move   X toggle   O reset 10", 0x80E0E0E0u, scale);
    g_screen.text(6, g_screen.h - rowh * 2 - 4,
                  "L1 -10  R1 +10  L2 -1000  R2 +1000   OPTIONS run   Touchpad quit",
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
        int rows = g_ntests + 1;                      // row 0 = ALL TESTS
        int move = 0;
        if (edge & PAD_UP) move = -1;
        else if (edge & PAD_DOWN) move = +1;
        else if (bn & PAD_UP)   { if (++rep > 8 && rep % 3 == 0) move = -1; }
        else if (bn & PAD_DOWN) { if (++rep > 8 && rep % 3 == 0) move = +1; }
        else rep = 0;
        if (move) { g_cursor = (g_cursor + move + rows) % rows; dirty = true; }

        int delta = 0;
        if (edge & PAD_L1) delta = -10;
        if (edge & PAD_R1) delta = +10;
        if (edge & PAD_L2) delta = -1000;
        if (edge & PAD_R2) delta = +1000;
        bool reset = (edge & PAD_CIRCLE) != 0;
        if (delta || reset) {
            dirty = true;
            int lo = (g_cursor == 0) ? 0 : g_cursor - 1;
            int hi = (g_cursor == 0) ? g_ntests - 1 : g_cursor - 1;
            for (int i = lo; i <= hi; i++) {
                int t = reset ? 10 : g_tests[i].tries + delta;
                if (t < 1) t = 1; if (t > 1000000) t = 1000000;
                g_tests[i].tries = t;
            }
        }

        if (edge & PAD_CROSS) {
            dirty = true;
            if (g_cursor == 0) {                      // toggle all selected
                bool all = true;
                for (int i = 0; i < g_ntests; i++) if (!g_tests[i].sel) all = false;
                for (int i = 0; i < g_ntests; i++) g_tests[i].sel = !all;
            } else {
                g_tests[g_cursor - 1].sel = !g_tests[g_cursor - 1].sel;
            }
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
    g_runs_total = 0;
    for (int i = 0; i < g_ntests; i++)
        if (g_tests[i].sel) {
            sel++; g_runs_total += g_tests[i].tries;
            if (g_tests[i].compute) need_compute = true;
        }
    if (g_runs_total < 1) g_runs_total = 1;
    g_runs_base = 0; g_runs_done = 0; g_screen.last_tick = 0;
    g_tick_step = g_runs_total / 200; if (g_tick_step < 1) g_tick_step = 1;
    tests_run = 0; tests_passed = 0; tests_failed = 0; tests_skipped = 0;
    g_screen.nlines = 0;
    sceKernelUsleep(200000);
    gpu_pool_reset();

    logf("========================================================\n");
    logf(" RACE SUITE  |  %d tests, %d total runs\n", sel, (int)g_runs_total);
    logf("========================================================\n");

    // Graphics-ring tests first so their results commit before any compute map.
    for (int i = 0; i < g_ntests; i++)
        if (g_tests[i].sel && !g_tests[i].compute) {
            g_tries = g_tests[i].tries; g_tests[i].fn();
        }

    if (need_compute) {
        if (g_cq.vqid <= 0) {
            logf("\n[compute] mapping ASC queue (may abort shadps4)...\n");
            if (!g_cq.init(0, 0))
                logf("[compute] queue map failed - skipping compute tests\n");
        }
        if (g_cq.vqid > 0)
            for (int i = 0; i < g_ntests; i++)
                if (g_tests[i].sel && g_tests[i].compute) {
                    g_tries = g_tests[i].tries; g_tests[i].fn();
                }
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
