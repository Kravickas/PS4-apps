// Direct memory release test: kernel return codes and the virtual mappings left behind.
// Results go to the screen and /data/dmem_results.txt (fallback /temp0/). Addresses are printed
// relative to each test's own block, so a PS4 log and a shadPS4 log diff line for line.
//   phys:  A allocated  F free
//   va:    M mapped, data intact   m mapped, data changed   n mapped, no CPU read
//          r mapped, backing released (not read)   . unmapped

#include <stddef.h>
#include <stdint.h>

extern "C" {
int32_t sceKernelAllocateDirectMemory(long search_start, long search_end, size_t len,
                                      size_t alignment, int memory_type, long* phys_out);
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, long phys,
                                 size_t alignment);
int32_t sceKernelMunmap(void* addr, size_t len);
int32_t sceKernelReleaseDirectMemory(long start, size_t len);
// OpenOrbis declares this void; libkernel returns the ioctl result (0x17904: mov eax, ebx).
int32_t sceKernelCheckedReleaseDirectMemory(long start, size_t len);
int32_t sceKernelDirectMemoryQuery(long offset, int32_t flags, void* info, size_t info_size);
int32_t sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int32_t* prot);
int32_t sceKernelEnableDmemAliasing(void);
long sceKernelGetDirectMemorySize(void);
int sceKernelUsleep(unsigned int usec);
int sceKernelOpen(const char* path, int flags, unsigned short mode);
long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
int sceKernelClose(int fd);
int sceKernelFsync(int fd);
int32_t sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void* param);
void sceVideoOutSetBufferAttribute(void* attr, uint32_t format, uint32_t tmode, uint32_t aspect,
                                   uint32_t w, uint32_t h, uint32_t pitch);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t start, void* const* addrs, int32_t num,
                                   const void* attr);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t buf, uint32_t mode, int64_t arg);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutGetFlipStatus(int32_t handle, void* status);
}

static void* my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d;
    while (n--)
        *p++ = (unsigned char)v;
    return d;
}

// ---- screen ----------------------------------------------------------------------------------

struct VideoBufAttr { // OrbisVideoOutBufferAttribute
    int32_t format, tmode, aspect;
    uint32_t width, height, pitch;
    uint64_t reserved[2];
};
struct VideoFlipStatus { // OrbisVideoOutFlipStatus
    uint64_t num, ptime, stime;
    int64_t flip_arg;
    uint64_t reserved[2];
    int32_t gpu_pending, pending, current_buffer;
    uint32_t r1;
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
#define SCR_COLS 120

struct Screen {
    int handle, w, h, cur;
    bool ok;
    long long flipid;
    uint32_t* fb[2];
    char lines[SCR_MAXLINES][SCR_COLS];
    int nlines;

    bool init() {
        ok = false;
        cur = 0;
        flipid = 0;
        nlines = 0;
        handle = sceVideoOutOpen(0xFF, 0, 0, 0);
        if (handle < 0)
            return false;
        w = 1920;
        h = 1080;
        const unsigned long fbsz = (unsigned long)w * h * 4ul;
        const unsigned long align = 0x200000ul;
        const unsigned long total = (fbsz * 2ul + align - 1) / align * align;
        long off = 0;
        if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3,
                                          &off) < 0)
            return false;
        void* base = 0;
        if (sceKernelMapDirectMemory(&base, total, 0x33, 0, off, align) < 0)
            return false;
        fb[0] = (uint32_t*)base;
        fb[1] = (uint32_t*)((char*)base + fbsz);
        VideoBufAttr attr;
        my_memset(&attr, 0, sizeof(attr));
        sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1u, 0u, (uint32_t)w, (uint32_t)h,
                                      (uint32_t)w);
        void* bufs[2] = {fb[0], fb[1]};
        if (sceVideoOutRegisterBuffers(handle, 0, bufs, 2, &attr) != 0)
            return false;
        sceVideoOutSetFlipRate(handle, 0);
        ok = true;
        return true;
    }

    void push(const char* s, int n) {
        int i = 0;
        while (i < n) {
            int j = i, t = 0;
            char tmp[SCR_COLS];
            while (j < n && s[j] != '\n') {
                if (t < SCR_COLS - 1)
                    tmp[t++] = s[j];
                j++;
            }
            tmp[t] = 0;
            if (t > 0) {
                if (nlines >= SCR_MAXLINES) {
                    for (int k = 1; k < SCR_MAXLINES; k++)
                        for (int c = 0; c < SCR_COLS; c++)
                            lines[k - 1][c] = lines[k][c];
                    nlines = SCR_MAXLINES - 1;
                }
                for (int c = 0; c <= t; c++)
                    lines[nlines][c] = tmp[c];
                nlines++;
            }
            i = (j < n) ? j + 1 : j;
        }
    }

    void glyph(uint32_t* dst, int px, int py, char ch, uint32_t col, int scale) {
        if (ch < 0x20 || ch > 0x7e)
            ch = '?';
        const unsigned char* g = FONT[ch - 0x20];
        for (int ry = 0; ry < 8; ry++) {
            const unsigned char row = g[ry];
            for (int rx = 0; rx < 8; rx++) {
                if (!(row & (1 << rx)))
                    continue;
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++) {
                        const int x = px + rx * scale + sx, y = py + ry * scale + sy;
                        if (x >= 0 && x < w && y >= 0 && y < h)
                            dst[(long)y * w + x] = col;
                    }
            }
        }
    }

    void present() {
        flipid++;
        sceVideoOutSubmitFlip(handle, cur, 1, flipid);
        VideoFlipStatus st;
        for (int t = 0; t < 300; t++) {
            my_memset(&st, 0, sizeof(st));
            sceVideoOutGetFlipStatus(handle, &st);
            if (st.flip_arg == flipid)
                break;
            sceKernelUsleep(100);
        }
        cur = 1 - cur;
    }

    static bool has(const char* s, const char* k) {
        for (; *s; s++) {
            int i = 0;
            while (k[i] && s[i] == k[i])
                i++;
            if (!k[i])
                return true;
        }
        return false;
    }

    void render() {
        if (!ok)
            return;
        uint32_t* dst = fb[cur];
        for (long i = 0, n = (long)w * h; i < n; i++)
            dst[i] = 0x80101018u;
        const int scale = nlines * 20 > h - 8 ? 1 : 2;
        const int gw = 8 * scale, rowh = 10 * scale;
        const int maxrows = (h - 8) / rowh;
        const int first = nlines > maxrows ? nlines - maxrows : 0;
        int y = 4;
        for (int li = first; li < nlines; li++) {
            const char* s = lines[li];
            uint32_t col = 0x80E0E0E0u;
            if (s[0] == '=')
                col = 0x8060C0F0u;
            else if (has(s, "WARN") || has(s, "SKIP"))
                col = 0x80F0C040u;
            int x = 6;
            for (const char* q = s; *q; q++) {
                glyph(dst, x, y, *q, col, scale);
                x += gw;
                if (x > w - gw)
                    break;
            }
            y += rowh;
        }
        present();
    }
};
static Screen g_screen;

// ---- log ------------------------------------------------------------------------------------
// Each line is opened, written, fsynced and closed, so it survives the process dying mid-test.

static const char* g_log_path = "/data/dmem_results.txt";

static void log_init() {
    int fd = sceKernelOpen(g_log_path, 0x0601, 0777);
    if (fd < 0) {
        g_log_path = "/temp0/dmem_results.txt";
        fd = sceKernelOpen(g_log_path, 0x0601, 0777);
    }
    if (fd >= 0)
        sceKernelClose(fd);
}

static void emit(const char* s, int n) {
    sceKernelWrite(1, s, (unsigned long)n);
    const int fd = sceKernelOpen(g_log_path, 0x0209, 0777);
    if (fd >= 0) {
        sceKernelWrite(fd, s, (unsigned long)n);
        sceKernelFsync(fd);
        sceKernelClose(fd);
    }
    if (g_screen.ok) {
        g_screen.push(s, n);
        g_screen.render();
    }
}

struct Line {
    char b[200];
    int p = 0;
    Line& s(const char* t) {
        while (*t && p < 190)
            b[p++] = *t++;
        return *this;
    }
    Line& c(char ch) {
        if (p < 190)
            b[p++] = ch;
        return *this;
    }
    Line& to(int col) {
        while (p < col && p < 190)
            b[p++] = ' ';
        return *this;
    }
    Line& hex(uint64_t v) {
        char t[16];
        int n = 0;
        do {
            const int d = (int)(v & 0xF);
            t[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
            v >>= 4;
        } while (v);
        while (n)
            c(t[--n]);
        return *this;
    }
    Line& dec(long long v) {
        if (v < 0) {
            c('-');
            v = -v;
        }
        char t[20];
        int n = 0;
        do {
            t[n++] = (char)('0' + v % 10);
            v /= 10;
        } while (v);
        while (n)
            c(t[--n]);
        return *this;
    }
    Line& shex(long long v) {
        if (v < 0) {
            c('-');
            return hex((uint64_t)-v);
        }
        return hex((uint64_t)v);
    }
    Line& ret(const char* tag, int32_t r) {
        s(tag).c('=');
        const int start = p;
        hex((uint32_t)r);
        return to(start + 9);
    }
    void end() {
        while (p > 0 && b[p - 1] == ' ')
            p--;
        b[p++] = '\n';
        emit(b, p);
    }
};

static void say(const char* t) {
    Line l;
    l.s(t).end();
}

// ---- dmem helpers ---------------------------------------------------------------------------

static const uint64_t PG = 0x4000; // direct memory granularity
static const uint64_t BIG = 0xFFFFFFFFFFFFC000ull;
static uint64_t g_dmem = 0;

struct DmemInfo { // OrbisQueryInfo
    uint64_t start, end;
    int32_t mtype, pad;
};

static long alloc_in(uint64_t lo, uint64_t hi, int pages, int type, int32_t* ret) {
    long pa = -1;
    const int32_t r =
        sceKernelAllocateDirectMemory((long)lo, (long)hi, pages * PG, PG, type, &pa);
    if (ret)
        *ret = r;
    return r == 0 ? pa : -1;
}

static long alloc(int pages, int type, int32_t* ret) {
    return alloc_in(0, g_dmem, pages, type, ret);
}

static bool allocated(uint64_t pa) {
    DmemInfo q;
    my_memset(&q, 0, sizeof(q));
    if (sceKernelDirectMemoryQuery((long)pa, 0, &q, sizeof(q)) != 0)
        return false;
    return q.start <= pa && pa < q.end;
}

static bool mapped(void* va, int32_t* prot) {
    void* s = 0;
    void* e = 0;
    int32_t p = 0;
    if (sceKernelQueryMemoryProtection(va, &s, &e, &p) != 0)
        return false;
    if (prot)
        *prot = p;
    return true;
}

static uint8_t* map(uint64_t pa, int pages, int32_t* ret) {
    void* va = 0;
    const int32_t r = sceKernelMapDirectMemory(&va, pages * PG, 0x3, 0, (long)pa, PG);
    if (ret)
        *ret = r;
    return r == 0 ? (uint8_t*)va : nullptr;
}

static void stamp(uint8_t* va, int pages, uint32_t tag) {
    for (int i = 0; i < pages; i++)
        *(volatile uint32_t*)(va + i * PG) = tag + i;
}

// One char per page. A page is read only if its backing is still allocated and CPU-readable.
static char va_state(uint8_t* va, uint64_t pa, uint32_t expect) {
    int32_t prot = 0;
    if (!mapped(va, &prot))
        return '.';
    if (!allocated(pa))
        return 'r';
    if (!(prot & 1))
        return 'n';
    return *(volatile uint32_t*)va == expect ? 'M' : 'm';
}

static Line& phys_str(Line& l, uint64_t pa, int pages) {
    l.s("phys=[");
    for (int i = 0; i < pages; i++)
        l.c(allocated(pa + i * PG) ? 'A' : 'F');
    return l.s("]");
}

// va pages map phys pages starting at pa_first.
static Line& va_str(Line& l, const char* tag, uint8_t* va, uint64_t pa_first, int pages,
                    uint32_t stamp_tag) {
    l.s(tag).s("=[");
    for (int i = 0; i < pages; i++)
        l.c(va ? va_state(va + i * PG, pa_first + i * PG, stamp_tag + i) : '-');
    return l.s("]");
}

static void unmap_all(uint8_t* va, int pages) {
    if (!va)
        return;
    for (int i = 0; i < pages; i++)
        if (mapped(va + i * PG, nullptr))
            sceKernelMunmap(va + i * PG, PG);
}

static void release_all(long pa, int pages) {
    if (pa < 0)
        return;
    for (int i = 0; i < pages; i++)
        if (allocated(pa + i * PG))
            sceKernelReleaseDirectMemory(pa + (long)(i * PG), PG);
}

static Line head(const char* id, const char* name) {
    Line l;
    l.s(id).to(5).s(name).c(' ').to(36);
    return l;
}

static void setup_failed(const char* id, const char* name, const char* what, int32_t r) {
    Line l = head(id, name);
    l.s("SKIP ").s(what).c(' ').ret("ret", r).end();
}

// ---- S: the probes themselves ---------------------------------------------------------------

static void q_line(const char* id, const char* name, long at, long ref) {
    DmemInfo q;
    my_memset(&q, 0, sizeof(q));
    const int32_t r = sceKernelDirectMemoryQuery(at, 0, &q, sizeof(q));
    Line l = head(id, name);
    l.ret("ret", r);
    if (r == 0)
        l.s("start=").shex((long long)(q.start - (uint64_t)ref)).s(" end=");
    if (r == 0)
        l.shex((long long)(q.end - (uint64_t)ref)).s(" mtype=").dec(q.mtype);
    l.end();
}

// [E0 type0][A type3][B type3][D type3][E1 type0], 4 pages each; offsets relative to B.
static void neighbours() {
    say("== S3-S7 query extent, neighbours A|B|D type 3 between type-0 walls, rel to B (0..10000)");
    int32_t r = 0;
    const long c = alloc(20, 3, &r);
    if (c < 0)
        return setup_failed("S3", "neighbour layout", "alloc scratch", r);
    sceKernelReleaseDirectMemory(c, 20 * PG);
    const int types[5] = {0, 3, 3, 3, 0};
    long blk[5];
    bool ok = true;
    for (int i = 0; i < 5; i++) {
        blk[i] = alloc_in(c + i * 4 * PG, c + (i + 1) * 4 * PG, 4, types[i], &r);
        ok = ok && blk[i] == (long)(c + i * 4 * PG);
    }
    if (ok) {
        const long b = blk[2];
        q_line("S3", "query B, all unmapped", b, b);
        uint8_t* va_a = map(blk[1], 4, &r);
        q_line("S4", "query B, A mapped", b, b);
        uint8_t* va_d = map(blk[3], 4, &r);
        q_line("S5", "query B, A and D mapped", b, b);
        q_line("S6", "query A (mapped)", blk[1], b);
        q_line("S7", "query D (mapped)", blk[3], b);
        unmap_all(va_a, 4);
        unmap_all(va_d, 4);
    } else {
        setup_failed("S3", "neighbour layout", "adjacent alloc", r);
    }
    for (int i = 0; i < 5; i++)
        release_all(blk[i], 4);
}

static bool sanity() {
    int32_t r = 0;
    const long pa = alloc(2, 3, &r);
    if (pa < 0) {
        setup_failed("S1", "alloc 2pg", "alloc", r);
        return false;
    }
    DmemInfo q0, q1;
    my_memset(&q0, 0, sizeof(q0));
    my_memset(&q1, 0, sizeof(q1));
    const int32_t r0 = sceKernelDirectMemoryQuery(pa, 0, &q0, sizeof(q0));
    const int32_t r1 = sceKernelDirectMemoryQuery(pa + (long)PG, 0, &q1, sizeof(q1));
    {
        Line l = head("S1", "query start / mid of alloc");
        l.ret("q0", r0).ret("q1", r1).s("start=").shex((long long)(q0.start - (uint64_t)pa));
        l.s(" end=").shex((long long)(q0.end - (uint64_t)pa)).s(" (block is 0..8000)");
        l.s(" mtype=").dec(q0.mtype).end();
    }
    const bool phys_ok = r0 == 0 && r1 == 0 && allocated(pa) && allocated(pa + PG);
    uint8_t* va = map(pa, 2, &r);
    if (!va) {
        setup_failed("S2", "map 2pg", "map", r);
        release_all(pa, 2);
        return false;
    }
    stamp(va, 2, 0x5A000000);
    int32_t prot = 0;
    const bool m = mapped(va, &prot);
    {
        Line l = head("S2", "map 2pg, query prot");
        l.ret("ret", r).s("prot=").hex((uint32_t)prot).c(' ');
        va_str(l, "va", va, pa, 2, 0x5A000000).end();
    }
    unmap_all(va, 2);
    release_all(pa, 2);
    if (!phys_ok || !m) {
        say("S    WARN probes unreliable: query of an allocated/mapped page failed");
        return false;
    }
    return true;
}

// ---- A: release return codes, unmapped memory -----------------------------------------------

enum Op { REL, CHK };

static int32_t release(Op op, long start, uint64_t len) {
    return op == REL ? sceKernelReleaseDirectMemory(start, len)
                     : sceKernelCheckedReleaseDirectMemory(start, len);
}

// 4-page block; optionally pre-free some pages; one release; report ret and the block.
static void a_case(const char* id, const char* name, Op op, uint64_t off, uint64_t len,
                   uint32_t prefree_mask = 0, bool twice = false) {
    int32_t r = 0;
    const long pa = alloc(4, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    for (int i = 0; i < 4; i++)
        if (prefree_mask & (1u << i))
            sceKernelReleaseDirectMemory(pa + (long)(i * PG), PG);
    const int32_t r1 = release(op, pa + (long)off, len);
    Line l = head(id, name);
    l.ret("ret", r1);
    if (twice)
        l.ret("ret2", release(op, pa + (long)off, len));
    phys_str(l, pa, 4).end();
    release_all(pa, 4);
}

// A page known to be free: allocate it, then release it.
static void a_free(const char* id, const char* name, Op op) {
    int32_t r = 0;
    const long pa = alloc(1, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    sceKernelReleaseDirectMemory(pa, PG);
    Line l = head(id, name);
    l.ret("ret", release(op, pa, PG));
    phys_str(l, pa, 1).end();
    release_all(pa, 1);
}

static void a_raw(const char* id, const char* name, Op op, uint64_t start, uint64_t len) {
    Line l = head(id, name);
    l.ret("ret", release(op, (long)start, len)).end();
}

static void run_a() {
    say("== A  release return codes (4-page block, unmapped)");
    a_case("A1", "rel  whole block", REL, 0, 4 * PG);
    a_case("A2", "rel  len 0", REL, 0, 0);
    a_case("A3", "rel  start +1000", REL, 0x1000, PG);
    a_case("A4", "rel  len +1000", REL, 0, PG + 0x1000);
    a_case("A5", "rel  start +1000, len 0", REL, 0x1000, 0);
    a_case("A6", "rel  page 1 only", REL, PG, PG);
    a_case("A7", "rel  whole block twice", REL, 0, 4 * PG, 0, true);
    a_case("A8", "rel  pages 2-3 already free", REL, 0, 4 * PG, 0xC);
    a_free("A9", "rel  free page", REL);
    a_raw("A10", "rel  start = dmem size", REL, g_dmem, PG);
    a_case("A11", "chk  whole block", CHK, 0, 4 * PG);
    a_case("A12", "chk  len 0", CHK, 0, 0);
    a_case("A13", "chk  start +1000", CHK, 0x1000, PG);
    a_case("A14", "chk  len +1000", CHK, 0, PG + 0x1000);
    a_case("A15", "chk  whole block twice", CHK, 0, 4 * PG, 0, true);
    a_case("A16", "chk  pages 2-3 already free", CHK, 0, 4 * PG, 0xC);
    a_free("A17", "chk  free page", CHK);
    a_raw("A18", "chk  start = dmem size", CHK, g_dmem, PG);
}

// ---- B: what release does to mappings of the released memory --------------------------------

// Allocate `pages`, map [map_first, map_first + map_pages), stamp, release, report.
static void b_case(const char* id, const char* name, Op op, int pages, int map_first,
                   int map_pages, uint64_t rel_off, uint64_t rel_len, uint32_t tag) {
    int32_t r = 0;
    const long pa = alloc(pages, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    const uint64_t map_pa = pa + map_first * PG;
    uint8_t* va = map(map_pa, map_pages, &r);
    if (!va) {
        setup_failed(id, name, "map", r);
        return release_all(pa, pages);
    }
    stamp(va, map_pages, tag);
    Line l = head(id, name);
    l.ret("ret", release(op, pa + (long)rel_off, rel_len));
    va_str(l, "va", va, map_pa, map_pages, tag).c(' ');
    phys_str(l, pa, pages).end();
    unmap_all(va, map_pages);
    release_all(pa, pages);
}

// One mapping over two adjacent allocations, released across their boundary.
static void b_split(const char* id, const char* name, int type_a, int type_b, uint32_t tag) {
    int32_t r = 0;
    const long scratch = alloc(8, 3, &r);
    if (scratch < 0)
        return setup_failed(id, name, "alloc scratch", r);
    sceKernelReleaseDirectMemory(scratch, 8 * PG);
    const uint64_t c = (uint64_t)scratch;
    int32_t ra = 0, rb = 0;
    const long a = alloc_in(c, c + 4 * PG, 4, type_a, &ra);
    const long b = alloc_in(c + 4 * PG, c + 8 * PG, 4, type_b, &rb);
    if (a != (long)c || b != (long)(c + 4 * PG)) {
        Line l = head(id, name);
        l.s("SKIP adjacent alloc ").ret("a", ra).ret("b", rb).end();
        release_all(a, 4);
        release_all(b, 4);
        return;
    }
    uint8_t* va = map(c, 8, &r);
    if (!va) {
        Line l = head(id, name);
        l.ret("map", r).s("one mapping over both refused").end();
        release_all(a, 4);
        release_all(b, 4);
        return;
    }
    stamp(va, 8, tag);
    Line l = head(id, name);
    l.ret("ret", sceKernelReleaseDirectMemory((long)(c + 2 * PG), 4 * PG));
    va_str(l, "va", va, c, 8, tag).c(' ');
    phys_str(l, c, 8).end();
    unmap_all(va, 8);
    release_all(a, 4);
    release_all(b, 4);
}

// The same memory mapped twice, then released.
static void b_alias(const char* id, const char* name, uint32_t tag) {
    int32_t r = 0;
    const long pa = alloc(2, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    uint8_t* v1 = map(pa, 2, &r);
    if (!v1) {
        setup_failed(id, name, "map 1", r);
        return release_all(pa, 2);
    }
    stamp(v1, 2, tag);
    int32_t r2 = 0;
    uint8_t* v2 = map(pa, 2, &r2);
    Line l = head(id, name);
    l.ret("map2", r2);
    if (v2) {
        l.ret("ret", sceKernelReleaseDirectMemory(pa, 2 * PG));
        va_str(l, "va1", v1, pa, 2, tag).c(' ');
        va_str(l, "va2", v2, pa, 2, tag).c(' ');
    }
    phys_str(l, pa, 2).end();
    unmap_all(v1, 2);
    unmap_all(v2, 2);
    release_all(pa, 2);
}

static void run_b() {
    say("== B  release of mapped memory (va: one char per mapped page)");
    b_case("B1", "rel  all 4, mapped all 4", REL, 4, 0, 4, 0, 4 * PG, 0x5A010000);
    b_case("B2", "rel  pages 1-2, mapped all 4", REL, 4, 0, 4, PG, 2 * PG, 0x5A020000);
    b_case("B3", "rel  page 0, mapped all 4", REL, 4, 0, 4, 0, PG, 0x5A030000);
    b_case("B4", "rel  2-5, mapped 4-7", REL, 8, 4, 4, 2 * PG, 4 * PG, 0x5A040000);
    b_case("B5", "rel  2-5, mapped 0-3", REL, 8, 0, 4, 2 * PG, 4 * PG, 0x5A050000);
    b_case("B6", "rel  1-6, mapped 2-5", REL, 8, 2, 4, PG, 6 * PG, 0x5A060000);
    b_case("B7", "chk  all 4, mapped all 4", CHK, 4, 0, 4, 0, 4 * PG, 0x5A070000);
    b_split("B8", "rel  2-5, 1 map, types 0+3", 0, 3, 0x5A080000);
    b_split("B9", "rel  2-5, 1 map, types 3+3", 3, 3, 0x5A090000);
    b_alias("B10", "map twice, rel", 0x5A0A0000);
    {
        Line l = head("B11", "enable dmem aliasing");
        l.ret("ret", sceKernelEnableDmemAliasing()).end();
    }
    b_alias("B12", "map twice after enable, rel", 0x5A0C0000);
}

// ---- C: oversized length ---------------------------------------------------------------------

static void run_c() {
    say("== C  oversized length, BIG = FFFFFFFFFFFFC000 (C4 frees a real allocation)");
    const uint64_t last = g_dmem - PG;
    a_raw("C1", "rel  last page, len BIG", REL, last, BIG);
    a_raw("C2", "chk  last page, len BIG", CHK, last, BIG);
    a_raw("C3", "rel  last page, len 1<<32", REL, last, 0x100000000ull);
    a_raw("C5", "rel  last page, len 1<<63", REL, last, 0x8000000000000000ull);
    a_raw("C6", "rel  last page, end = 2^64-4000", REL, last, 0ull - 0x4000 - last);
    a_raw("C7", "rel  last page, end = 2^64 (wraps to 0)", REL, last, 0ull - last);
    static const int ladder[] = {33, 34, 35, 36, 40, 48, 56, 62};
    for (int i = 0; i < 8; i++) {
        char id[4] = {'L', (char)('1' + i), 0, 0};
        char name[32] = "rel  last page, len 1<<";
        name[23] = (char)('0' + ladder[i] / 10);
        name[24] = (char)('0' + ladder[i] % 10);
        name[25] = 0;
        a_raw(id, name, REL, last, 1ull << ladder[i]);
    }
    a_raw("C8", "rel  last page, len 7FFFFFFFFFFFC000", REL, last, 0x7FFFFFFFFFFFC000ull);
    a_raw("C9", "rel  last page, end = 2^63-4000", REL, last, 0x7FFFFFFFFFFFC000ull - last);
    a_raw("C10", "rel  last page, end = 2^63", REL, last, 0x8000000000000000ull - last);

    int32_t r0 = 0, r1 = 0, r2 = 0;
    const long lo = alloc(1, 3, &r0);
    const long mid = alloc(1, 3, &r1);
    const long hi = alloc(1, 3, &r2);
    if (lo < 0 || mid < 0 || hi < 0) {
        Line l = head("C4", "rel  own page, len BIG");
        l.s("SKIP alloc ").ret("a", r0).ret("b", r1).ret("c", r2).end();
    } else {
        say("C4   starting");
        const int32_t r = sceKernelReleaseDirectMemory(mid, BIG);
        Line l = head("C4", "rel  own page, len BIG");
        l.ret("ret", r);
        l.s("first=").c(lo < mid ? '<' : '>').c(allocated(lo) ? 'A' : 'F');
        l.s(" self=").c(allocated(mid) ? 'A' : 'F');
        l.s(" third=").c(hi < mid ? '<' : '>').c(allocated(hi) ? 'A' : 'F').end();
    }
    release_all(lo, 1);
    release_all(mid, 1);
    release_all(hi, 1);
}

int main(void) {
    g_screen.init();
    log_init();
    g_dmem = (uint64_t)sceKernelGetDirectMemorySize();
    say("===== DMEM RELEASE TEST =====");
    {
        Line l;
        l.s("dmem size=").hex(g_dmem).s("  page=").hex(PG).s("  screen=").c(g_screen.ok ? '1' : '0');
        l.end();
    }
    say("phys: A alloc F free | va: M ok m changed n no-read r backing freed . unmapped");
    say("== S  probes");
    if (sanity()) {
        neighbours();
        run_a();
        run_b();
        run_c();
    }
    say("===== DONE =====");
    for (;;)
        sceKernelUsleep(1000000);
    return 0;
}
