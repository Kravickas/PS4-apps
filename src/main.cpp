// ============================================================================
//  PS4 VO flip / IRQ path test  -  standalone homebrew
//
//  Same binary on real hardware and in shadPS4, same output lines, so the two
//  logs diff directly. Results are shown on screen and written to
//  /data/irq_results.txt (falls back to /temp0) and fd 1.
//
//  From libSceGnmDriver disassembly: the reserved 64-dword block at the tail of
//  a dcb (header 0xc03e1000 = IT_NOP with 63 payload dwords, PrepareFlip marker
//  in payload[0]) is rewritten by PatchFlipRequest into
//  WRITE_DATA(vo_label[buf_idx] = 1) followed by NOP(PatchedFlip 0x68750776).
//  sceGnmInsertWaitFlipDone emits WAIT_REG_MEM(func=equal, ref=0) on the same
//  label, so 1 = buffer locked, 0 = released.
//
//  The dcb needs nothing else: no shaders, no draws, no register state. The UI
//  presents through the same EOP flip path, so the app has exactly one flip
//  path and the UI never competes with what is being measured.
//
//  Controls:  CROSS = run tests again    TRIANGLE = clear log
// ============================================================================

#include <stdint.h>
#include <stddef.h>

extern "C" {
    int32_t sceKernelAllocateDirectMemory(long search_start, long search_end, size_t len,
                                          size_t alignment, int memory_type, long* phys_addr_out);
    int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags,
                                     long phys_addr, size_t alignment);
    long    sceKernelGetDirectMemorySize(void);
    int     sceKernelUsleep(unsigned int usec);
    uint64_t sceKernelGetProcessTime(void);
    int     sceKernelOpen(const char* path, int flags, unsigned short mode);
    long    sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int     sceKernelClose(int fd);
    int     sceKernelFsync(int fd);

    int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, const void* param);
    void    sceVideoOutSetBufferAttribute(void* attr, uint32_t format, uint32_t tmode,
                                          uint32_t aspect, uint32_t w, uint32_t h, uint32_t pitch);
    int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex, void* const* addrs,
                                       int32_t num, const void* attr);
    int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
    int32_t sceVideoOutGetFlipStatus(int32_t handle, void* status);
    int32_t sceVideoOutGetBufferLabelAddress(int32_t handle, uintptr_t* label_addr);

    int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, uint32_t* dcb_addrs[],
                                              uint32_t* dcb_sizes, uint32_t* ccb_addrs[],
                                              uint32_t* ccb_sizes, uint32_t vo_handle,
                                              uint32_t buf_idx, uint32_t flip_mode,
                                              int64_t flip_arg);

    int32_t scePadInit(void);
    int32_t scePadOpen(int32_t userID, int32_t type, int32_t index, void* param);
    int32_t scePadReadState(int32_t handle, void* data);
    int32_t sceUserServiceInitialize(void* params);
    int32_t sceUserServiceGetInitialUser(int32_t* userId);
}

struct VideoBufAttr {
    int32_t format; int32_t tmode; int32_t aspect;
    uint32_t width; uint32_t height; uint32_t pitch; uint64_t reserved[2];
};

struct VideoFlipStatus {
    uint64_t num, ptime, stime; int64_t flipArg; uint64_t reserved[2];
    int32_t numGpuFlipPending, numFlipPending, currentBuffer; uint32_t r1;
};

static void* my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d;
    while (n--) *p++ = (unsigned char)v;
    return d;
}

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

#define SCR_MAXLINES 160
#define NBUF 3

// forward: the UI presents through the same EOP flip path the tests exercise
static int submit_eop_flip(int buf_idx, int64_t flip_arg);

static uintptr_t g_label_base = 0;
static bool      g_ui_hold    = false;   // set while a test runs: buffer log, don't present

static inline volatile uint64_t* vo_label(int i) {
    return (volatile uint64_t*)(g_label_base + (uintptr_t)i * 8u);
}

struct Screen {
    int handle; bool ok; int w, h; int cur; long long flipid;
    uint32_t* fb[NBUF];
    char lines[SCR_MAXLINES][120]; int nlines;

    static int u2s(char* b, int p, int v) {   // append unsigned decimal
        char t[12]; int n = 0;
        if (v <= 0) t[n++] = '0';
        while (v > 0) { t[n++] = (char)('0' + v % 10); v /= 10; }
        while (n > 0) b[p++] = t[--n];
        return p;
    }

    bool init() {
        ok = false; cur = 0; flipid = 0; nlines = 0;
        handle = sceVideoOutOpen(0xFF, 0, 0, 0);
        if (handle < 0) return false;
        w = 1920; h = 1080;
        unsigned long fbsz = (unsigned long)w * (unsigned long)h * 4ul;
        unsigned long align = 0x200000ul;
        unsigned long total = (fbsz * (unsigned long)NBUF + align - 1) / align * align;
        long off = 0;
        if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3, &off) < 0)
            return false;
        void* base = 0;
        if (sceKernelMapDirectMemory(&base, total, 0x33, 0, off, align) < 0) return false;
        for (int b = 0; b < NBUF; b++) fb[b] = (uint32_t*)((char*)base + fbsz * (unsigned long)b);
        VideoBufAttr attr; my_memset(&attr, 0, sizeof(attr));
        sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1u, 0u,
                                      (uint32_t)w, (uint32_t)h, (uint32_t)w);   // LINEAR
        void* bufs[NBUF] = { fb[0], fb[1], fb[2] };
        if (sceVideoOutRegisterBuffers(handle, 0, bufs, NBUF, &attr) != 0) return false;
        sceVideoOutSetFlipRate(handle, 0);
        if (sceVideoOutGetBufferLabelAddress(handle, &g_label_base) != 16) return false;
        if (g_label_base == 0) return false;
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

    // Present the UI with an EOP flip so the app has a single flip path.
    void present() {
        if (!ok) return;
        flipid++;
        if (submit_eop_flip(cur, flipid) != 0) return;
        for (int t = 0; t < 400; t++) {
            VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
            sceVideoOutGetFlipStatus(handle, &st);
            if (st.flipArg == flipid) break;
            sceKernelUsleep(100);
        }
        cur = (cur + 1) % NBUF;
    }

    void render() {
        if (!ok) return;
        clear(0x80101018u);
        uint32_t* dst = fb[cur];
        int scale = 2;
        if (nlines * (10 * scale) > h - 8) scale = 1;
        int gw = 8 * scale, rowh = 10 * scale;
        int maxrows = (h - 8) / rowh;
        int first = (nlines > maxrows) ? nlines - maxrows : 0;
        int y = 4;
        for (int li = first; li < nlines; li++) {
            const char* s = lines[li];
            uint32_t col = 0x80E0E0E0u;
            for (const char* q = s; *q; q++) {
                if (q[0]=='D'&&q[1]=='A'&&q[2]=='T'&&q[3]=='A') { col = 0x8040E060u; break; }
                if ((q[0]=='F'&&q[1]=='A'&&q[2]=='I'&&q[3]=='L') ||
                    (q[0]=='S'&&q[1]=='K'&&q[2]=='I'&&q[3]=='P')) { col = 0x80E05050u; break; }
            }
            int x = 6;
            for (const char* q = s; *q; q++) {
                glyph(dst, x, y, *q, col, scale); x += gw; if (x > w - gw) break;
            }
            y += rowh;
        }
        present();
    }
};

static Screen g_screen;

// ---------------------------------------------------------------- logging
static const char* g_log_path = "/data/irq_results.txt";

static void log_init() {
    int fd = sceKernelOpen(g_log_path, 0x0601, 0777);            // create + truncate
    if (fd < 0) {
        g_log_path = "/temp0/irq_results.txt";
        fd = sceKernelOpen(g_log_path, 0x0601, 0777);
    }
    if (fd >= 0) sceKernelClose(fd);
}

static int put_dec(char* b, int p, long long v) {
    if (v < 0) { b[p++] = '-'; v = -v; }
    char t[24]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v > 0) { t[n++] = (char)('0' + (int)(v % 10)); v /= 10; }
    while (n > 0) b[p++] = t[--n];
    return p;
}

static int put_hex(char* b, int p, unsigned long long v) {
    char t[20]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v > 0) { int d = (int)(v & 0xf); t[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10); v >>= 4; }
    while (n > 0) b[p++] = t[--n];
    return p;
}

// %d int, %l long long, %X hex, %s string
static void logf(const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char buf[512];
    int p = 0;
    for (const char* c = fmt; *c && p < (int)sizeof(buf) - 24; c++) {
        if (*c != '%') { buf[p++] = *c; continue; }
        c++;
        if (*c == 'd')      p = put_dec(buf, p, __builtin_va_arg(ap, int));
        else if (*c == 'l') p = put_dec(buf, p, __builtin_va_arg(ap, long long));
        else if (*c == 'X') p = put_hex(buf, p, __builtin_va_arg(ap, unsigned long long));
        else if (*c == 's') {
            const char* s = __builtin_va_arg(ap, const char*);
            while (*s && p < (int)sizeof(buf) - 2) buf[p++] = *s++;
        } else buf[p++] = *c;
    }
    __builtin_va_end(ap);

    sceKernelWrite(1, buf, (unsigned long)p);
    int fd = sceKernelOpen(g_log_path, 0x0209, 0777);            // append
    if (fd >= 0) {
        sceKernelWrite(fd, buf, (unsigned long)p);
        sceKernelFsync(fd);
        sceKernelClose(fd);
    }
    g_screen.push(buf, p);
    if (!g_ui_hold) g_screen.render();
}

// ---------------------------------------------------------------- gpu memory
#define GPU_PAGE  0x10000ul
#define GPU_PAGES 64

static uint8_t*     g_pool = 0;
static unsigned long g_pool_off = 0;

static bool pool_init() {
    long phys = 0;
    unsigned long total = GPU_PAGES * GPU_PAGE;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, GPU_PAGE, 3, &phys) != 0)
        return false;
    void* ptr = 0;
    if (sceKernelMapDirectMemory(&ptr, total, 0x33, 0, phys, GPU_PAGE) != 0) return false;
    g_pool = (uint8_t*)ptr;
    my_memset(g_pool, 0, total);
    g_pool_off = 0;
    return true;
}

static void* pool_alloc(unsigned long size) {
    unsigned long off = (g_pool_off + 255ul) & ~255ul;           // dcb: 256-byte aligned
    if (off + size > GPU_PAGES * GPU_PAGE) off = 0;              // wrap; earlier dcbs are done
    g_pool_off = off + size;
    return g_pool + off;
}

// ---------------------------------------------------------------- flip submit
#define IT_NOP              0x10u
#define PREPARE_FLIP_MARKER 0x68750777u                          // PM4CmdNop::PrepareFlip

static inline uint32_t pm4_hdr(uint32_t opcode, uint32_t count) {
    return (3u << 30) | ((count - 1u) << 16) | ((opcode & 0xFFu) << 8);
}

// The block must be the LAST 64 dwords of the dcb: PatchFlipRequest indexes
// cmdbuf[size_dw - 64], with size_dw = dcb_size_in_bytes / 4.
static int submit_eop_flip(int buf_idx, int64_t flip_arg) {
    uint32_t* dcb = (uint32_t*)pool_alloc(64 * sizeof(uint32_t));
    if (!dcb) return -1;
    dcb[0] = pm4_hdr(IT_NOP, 63);                                // 0xc03e1000
    dcb[1] = PREPARE_FLIP_MARKER;
    for (int i = 2; i < 64; i++) dcb[i] = 0;
    uint32_t* dp[1] = { dcb };
    uint32_t  ds[1] = { 64u * sizeof(uint32_t) };
    return sceGnmSubmitAndFlipCommandBuffers(1, dp, ds, 0, 0, (uint32_t)g_screen.handle,
                                             (uint32_t)buf_idx, 1u, flip_arg);
}

static bool wait_settled(uint32_t timeout_us) {
    while (timeout_us > 0) {
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_screen.handle, &st);
        if (st.numFlipPending <= 0) return true;
        sceKernelUsleep(200);
        timeout_us = (timeout_us > 200u) ? timeout_us - 200u : 0u;
    }
    return false;
}

// ---------------------------------------------------------------- trace
struct Sample { uint32_t t, l0, l1, l2; int32_t arg, cur, pend, gpu; };

static Sample g_trace[64];
static int    g_ntrace = 0;

static void trace_reset() { g_ntrace = 0; }

static void trace_sample(uint64_t t0) {
    VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
    sceVideoOutGetFlipStatus(g_screen.handle, &st);
    Sample s;
    s.t    = (uint32_t)(sceKernelGetProcessTime() - t0);
    s.l0   = (uint32_t)*vo_label(0);
    s.l1   = (uint32_t)*vo_label(1);
    s.l2   = (uint32_t)*vo_label(2);
    s.arg  = (int32_t)st.flipArg;
    s.cur  = st.currentBuffer;
    s.pend = st.numFlipPending;
    s.gpu  = st.numGpuFlipPending;
    if (g_ntrace > 0) {
        Sample& q = g_trace[g_ntrace - 1];
        if (q.l0 == s.l0 && q.l1 == s.l1 && q.l2 == s.l2 && q.arg == s.arg &&
            q.cur == s.cur && q.pend == s.pend && q.gpu == s.gpu) return;      // changes only
    }
    if (g_ntrace < (int)(sizeof(g_trace) / sizeof(g_trace[0]))) g_trace[g_ntrace++] = s;
}

static void trace_dump() {
    for (int i = 0; i < g_ntrace; i++) {
        Sample& s = g_trace[i];
        logf("   t=%d l0=%d l1=%d l2=%d arg=%d cur=%d pend=%d gpu=%d\n",
             (int)s.t, (int)s.l0, (int)s.l1, (int)s.l2, s.arg, s.cur, s.pend, s.gpu);
    }
}

// ---------------------------------------------------------------- tests
// FLIP-1: lifecycle of a single EOP flip.
static void test_flip_basic() {
    logf("[FLIP-1] single EOP flip label lifecycle\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }
    trace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    trace_sample(t0);
    int rc = submit_eop_flip(0, 901);
    if (rc != 0) { logf("  FAIL submit rc=%X\n", (unsigned long long)(uint32_t)rc); return; }
    for (int i = 0; i < 10000; i++) {
        trace_sample(t0);
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_screen.handle, &st);
        if (st.flipArg == 901 && st.numFlipPending <= 0) break;
        sceKernelUsleep(200);
    }
    trace_sample(t0);
    trace_dump();
    logf("  DATA label0_after_own_flip=%d (displayed buffer expected to stay locked)\n",
         (int)*vo_label(0));
}

// FLIP-2: the decisive probe.
// A->buf0, B->buf1, C->buf0 back to back with no WaitFlipDone, so buf0 is
// re-armed (label driven back to 1 by C) while it is still displayed. When B is
// presented buf0 stops being displayed: an unconditional label clear reads 0
// even though C is still pending, a conditional clear leaves it at 1.
static void test_flip_rearm() {
    logf("[FLIP-2] re-armed buffer replaced\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }
    trace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    trace_sample(t0);
    int rA = submit_eop_flip(0, 1001); trace_sample(t0);
    int rB = submit_eop_flip(1, 1002); trace_sample(t0);
    int rC = submit_eop_flip(0, 1003); trace_sample(t0);
    if (rA != 0 || rB != 0 || rC != 0) {
        logf("  FAIL submit rc A=%X B=%X C=%X\n", (unsigned long long)(uint32_t)rA,
             (unsigned long long)(uint32_t)rB, (unsigned long long)(uint32_t)rC);
        return;
    }
    // captured live so the answer never depends on trace-buffer capacity
    int l0_at_B = -1, l0_late_B = -1;
    uint64_t t_B = 0;
    for (int i = 0; i < 15000; i++) {
        trace_sample(t0);
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
    trace_sample(t0);
    trace_dump();
    logf("  DATA label0_at_B=%d label0_3ms_after_B=%d\n", l0_at_B, l0_late_B);
    logf("  DATA 1=re-arm survives (conditional clear) 0=unconditional clear\n");
    wait_settled(2000000);
}

// FLIP-3: queue depth and the full return code.
// PatchFlipRequest maps sceVideoOutSubmitEopFlip's 0x80290012 to 0x80D11081.
static void test_flip_queue_full() {
    logf("[FLIP-3] flip queue depth and full return code\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }
    int n_ok = 0, first_err = 0;
    for (int i = 0; i < 32; i++) {
        int rc = submit_eop_flip(i % NBUF, 2000 + i);
        if (rc == 0) { n_ok++; continue; }
        first_err = rc;
        break;
    }
    logf("  DATA accepted=%d first_err=%X (0x80D11081 = queue full)\n",
         n_ok, (unsigned long long)(uint32_t)first_err);
    wait_settled(4000000);
}

// FLIP-4: sustained rapid reuse. This is the shape that makes shadPS4's
// "Out of order flip IRQ" assert fire: the presenter clears a re-armed buffer's
// label between the GPU write and the flip marker. On hardware it is a
// throughput test and nothing more.
static void test_flip_stress() {
    logf("[FLIP-4] sustained rapid buffer reuse\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }
    const int TARGET = 600;
    int sent = 0, full = 0, err = 0, last_err = 0;
    uint64_t t0 = sceKernelGetProcessTime();
    for (int i = 0; i < TARGET; i++) {
        int rc = submit_eop_flip(i % NBUF, 3000 + i);
        if (rc == 0) { sent++; continue; }
        if ((uint32_t)rc == 0x80D11081u) {                        // queue full: back off
            full++;
            sceKernelUsleep(2000);
            i--;
            continue;
        }
        err++; last_err = rc;
        break;
    }
    uint64_t dt = sceKernelGetProcessTime() - t0;
    wait_settled(5000000);
    logf("  DATA sent=%d requeues=%d errors=%d last_err=%X us=%l\n",
         sent, full, err, (unsigned long long)(uint32_t)last_err, (long long)dt);
    logf("  DATA labels l0=%d l1=%d l2=%d\n",
         (int)*vo_label(0), (int)*vo_label(1), (int)*vo_label(2));
    logf("  DATA reached this line = no flip IRQ assert\n");
}

static void run_all() {
    g_ui_hold = true;                        // buffer the log, keep the port to ourselves
    logf("=== PS4 VO flip / IRQ test ===\n");
    logf("vo=%d label_base=%X buffers=%d\n",
         (int)g_screen.handle, (unsigned long long)g_label_base, NBUF);
    submit_eop_flip(0, 1);                   // establish a displayed buffer
    wait_settled(2000000);
    test_flip_basic();
    g_ui_hold = false; g_screen.render(); g_ui_hold = true;
    test_flip_rearm();
    g_ui_hold = false; g_screen.render(); g_ui_hold = true;
    test_flip_queue_full();
    g_ui_hold = false; g_screen.render(); g_ui_hold = true;
    test_flip_stress();
    g_ui_hold = false;
    logf("=== done. CROSS = run again, TRIANGLE = clear ===\n");
}

// ---------------------------------------------------------------- entry
int main(void) {
    int32_t user = 0xFF;
    sceUserServiceInitialize(0);
    sceUserServiceGetInitialUser(&user);
    log_init();

    if (!g_screen.init()) {
        logf("FATAL video out init failed\n");
        for (;;) sceKernelUsleep(1000000);
    }
    if (!pool_init()) {
        logf("FATAL gpu pool alloc failed\n");
        for (;;) sceKernelUsleep(1000000);
    }

    scePadInit();
    int pad = scePadOpen(user, 0, 0, 0);

    run_all();

    uint32_t prev = 0;
    for (;;) {
        PadState ps; my_memset(&ps, 0, sizeof(ps));
        if (pad >= 0 && scePadReadState(pad, &ps) == 0) {
            uint32_t pressed = ps.buttons & ~prev;
            prev = ps.buttons;
            if (pressed & PAD_CROSS) {
                log_init();                              // fresh results file per run
                g_screen.nlines = 0;
                run_all();
            } else if (pressed & PAD_TRIANGLE) {
                g_screen.nlines = 0;
                g_screen.render();
            }
        }
        sceKernelUsleep(16000);
    }
    return 0;
}
