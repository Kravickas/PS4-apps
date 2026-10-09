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
int32_t sceKernelMtypeprotect(const void* addr, size_t len, int32_t mtype, int32_t prot);
int32_t sceKernelMprotect(const void* addr, size_t len, int32_t prot);
// OpenOrbis declares the last two by value; libkernel stores through them (0x19019, 0x19027).
int32_t sceKernelAvailableDirectMemorySize(long search_start, long search_end, size_t alignment,
                                           long* phys_out, size_t* size_out);
// Not in the OpenOrbis headers; argument registers from libkernel 0x19d80 / 0x19f30.
int32_t sceKernelMemoryPoolExpand(uint64_t search_start, uint64_t search_end, uint64_t len,
                                  uint64_t alignment, uint64_t* phys_out);
int32_t sceKernelMemoryPoolReserve(void* addr_in, uint64_t len, uint64_t alignment, int32_t flags,
                                   void** addr_out);
int32_t sceKernelMemoryPoolCommit(void* addr, uint64_t len, int32_t type, int32_t prot,
                                  int32_t flags);
int32_t sceKernelMemoryPoolDecommit(void* addr, uint64_t len, int32_t flags);
// libkernel 0x19e80: ioctl 0x4010a802 into 16 bytes, copies min(size, 0x10); no null check.
int32_t sceKernelMemoryPoolGetBlockStats(void* stats, size_t size);
int32_t sceKernelGetDirectMemoryType(long start, int32_t* type, long* region_start,
                                     long* region_end);
int32_t sceKernelVirtualQuery(const void* addr, int32_t flags, void* info, size_t info_size);
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
    long fb_phys;
    unsigned long fb_len;
    long long flipid;
    uint32_t* fb[2];
    char lines[SCR_MAXLINES][SCR_COLS];
    int nlines;

    bool init() {
        ok = false;
        fb_phys = -1;
        fb_len = 0;
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
        fb_phys = off;
        fb_len = total;
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

static long alloc_al(uint64_t lo, uint64_t hi, int pages, uint64_t align, int type,
                     int32_t* ret) {
    long pa = -1;
    const int32_t r =
        sceKernelAllocateDirectMemory((long)lo, (long)hi, pages * PG, align, type, &pa);
    if (ret)
        *ret = r;
    return r == 0 ? pa : -1;
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

// ---- D: release edges -----------------------------------------------------------------------

// Block of `pages`; pre-free pages in `prefree`; one release; report the whole block.
static void d_block(const char* id, const char* name, Op op, int pages, uint32_t prefree,
                    uint64_t off, uint64_t len) {
    int32_t r = 0;
    const long pa = alloc(pages, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    for (int i = 0; i < pages; i++)
        if (prefree & (1u << i))
            sceKernelReleaseDirectMemory(pa + (long)(i * PG), PG);
    Line l = head(id, name);
    l.ret("ret", release(op, pa + (long)off, len));
    phys_str(l, pa, pages).end();
    release_all(pa, pages);
}

// Pages 4-5 mapped and outside the released range; a hole at 2-3 inside it.
static void d_outside_mapped(const char* id, const char* name) {
    int32_t r = 0;
    const long pa = alloc(6, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    uint8_t* va = map(pa + 4 * PG, 2, &r);
    if (!va) {
        setup_failed(id, name, "map", r);
        return release_all(pa, 6);
    }
    stamp(va, 2, 0x5A0D0000);
    sceKernelReleaseDirectMemory(pa + (long)(2 * PG), 2 * PG);
    Line l = head(id, name);
    l.ret("ret", sceKernelReleaseDirectMemory(pa, 4 * PG));
    va_str(l, "va45", va, pa + 4 * PG, 2, 0x5A0D0000).c(' ');
    phys_str(l, pa, 6).end();
    unmap_all(va, 2);
    release_all(pa, 6);
}

static void d_own_page(const char* id, const char* name, Op op, uint64_t len) {
    int32_t r = 0;
    const long pa = alloc(1, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    Line l = head(id, name);
    l.ret("ret", release(op, pa, len));
    phys_str(l, pa, 1).end();
    release_all(pa, 1);
}

static void run_d() {
    say("== D  release edges (block pages outside the released range must stay A)");
    const uint64_t last = g_dmem - PG;
    a_raw("D1", "chk  last page, len 1<<63", CHK, last, 0x8000000000000000ull);
    a_raw("D2", "chk  last page, len 7FFF..C000", CHK, last, 0x7FFFFFFFFFFFC000ull);
    a_raw("D3", "rel  start 1<<63, len 4000", REL, 0x8000000000000000ull, PG);
    a_raw("D4", "chk  start 1<<63, len 4000", CHK, 0x8000000000000000ull, PG);
    a_raw("D5", "rel  start FFFF..C000, len 4000", REL, 0xFFFFFFFFFFFFC000ull, PG);
    a_raw("D6", "chk  start FFFF..C000, len 4000", CHK, 0xFFFFFFFFFFFFC000ull, PG);
    d_block("D7", "rel  0-3 of 6, hole at 2-3", REL, 6, 0xC, 0, 4 * PG);
    d_block("D8", "chk  0-3 of 6, hole at 2-3", CHK, 6, 0xC, 0, 4 * PG);
    d_block("D9", "rel  0-3 of 6, hole at 0-1", REL, 6, 0x3, 0, 4 * PG);
    d_block("D10", "rel  0-3 of 8, holes at 1,3", REL, 8, 0xA, 0, 4 * PG);
    d_block("D11", "rel  1-4 of 6, hole at 2", REL, 6, 0x4, PG, 4 * PG);
    d_outside_mapped("D12", "rel  0-3, hole 2-3, 4-5 mapped");
    d_own_page("D13", "chk  own page, len 1<<62", CHK, 1ull << 62);
    d_own_page("D14", "rel  own page, len 0", REL, 0);
}

// ---- Q: query edges --------------------------------------------------------------------------

static void q_raw(const char* id, const char* name, long at, int32_t flags, size_t size,
                  long ref) {
    DmemInfo q;
    my_memset(&q, 0, sizeof(q));
    const int32_t r = sceKernelDirectMemoryQuery(at, flags, &q, size);
    Line l = head(id, name);
    l.ret("ret", r);
    if (r == 0) {
        l.s("start=").shex((long long)(q.start - (uint64_t)ref)).s(" end=");
        if (q.end == 0)
            l.s("unset");
        else
            l.shex((long long)(q.end - (uint64_t)ref));
        l.s(" mtype=").dec(q.mtype);
    }
    l.end();
}

// Contiguous 4-page blocks of the given types; returns the base or -1. Unused slots: type -1.
static long build(int n, const int* types, long* blk, uint64_t align) {
    int32_t r = 0;
    const long c = alloc_al(0, g_dmem, n * 4, align, 3, &r);
    if (c < 0)
        return -1;
    sceKernelReleaseDirectMemory(c, n * 4 * PG);
    bool ok = true;
    for (int i = 0; i < n; i++) {
        blk[i] = -1;
        if (types[i] < 0)
            continue;
        blk[i] = alloc_in(c + i * 4 * PG, c + (i + 1) * 4 * PG, 4, types[i], &r);
        ok = ok && blk[i] == (long)(c + i * 4 * PG);
    }
    if (!ok) {
        for (int i = 0; i < n; i++)
            release_all(blk[i], 4);
        return -1;
    }
    return c;
}

static void run_q() {
    say("== Q  query edges");
    {
        // [hole][A t3][B t3][wall t0]; offsets relative to A
        const int t[4] = {-1, 3, 3, 0};
        long blk[4];
        const long c = build(4, t, blk, PG);
        if (c < 0) {
            say("Q1   SKIP layout");
        } else {
            q_raw("Q1", "find-next from hole before A|B", c, 1, sizeof(DmemInfo), blk[1]);
            q_raw("Q2", "exact query of the hole", c, 0, sizeof(DmemInfo), blk[1]);
            q_raw("Q3", "find-next from inside B", blk[2], 1, sizeof(DmemInfo), blk[1]);
            for (int i = 0; i < 4; i++)
                release_all(blk[i], 4);
        }
    }
    {
        // [wall t3][X t0][Y t0][wall t3]; offsets relative to X
        const int t[4] = {3, 0, 0, 3};
        long blk[4];
        const long c = build(4, t, blk, PG);
        if (c < 0) {
            say("Q4   SKIP layout");
        } else {
            q_raw("Q4", "type 0 run: query Y", blk[2], 0, sizeof(DmemInfo), blk[1]);
            int32_t r = 0;
            uint8_t* vx = map(blk[1], 4, &r);
            q_raw("Q5", "type 0 run: query Y, X mapped", blk[2], 0, sizeof(DmemInfo), blk[1]);
            unmap_all(vx, 4);
            for (int i = 0; i < 4; i++)
                release_all(blk[i], 4);
        }
    }
    {
        // [A t3][8-page hole][B t3]; offsets relative to A
        const int t[4] = {3, -1, -1, 3};
        long blk[4];
        const long c = build(4, t, blk, PG);
        if (c < 0) {
            say("Q12  SKIP layout");
        } else {
            const long hole = c + (long)(4 * PG);
            q_raw("Q12", "find-next at hole start (A end)", hole, 1, sizeof(DmemInfo), c);
            q_raw("Q13", "find-next at hole +4000", hole + (long)PG, 1, sizeof(DmemInfo), c);
            q_raw("Q14", "find-next at hole +10000", hole + (long)(4 * PG), 1, sizeof(DmemInfo), c);
            q_raw("Q15", "find-next at last hole page", hole + (long)(7 * PG), 1,
                  sizeof(DmemInfo), c);
            q_raw("Q16", "find-next at A end - 4000", hole - (long)PG, 1, sizeof(DmemInfo), c);
            for (int i = 0; i < 4; i++)
                release_all(blk[i], 4);
        }
    }
    int32_t r = 0;
    const long pa = alloc(2, 3, &r);
    if (pa < 0) {
        setup_failed("Q6", "query edges", "alloc", r);
        return;
    }
    q_raw("Q6", "offset +1000 inside alloc", pa + 0x1000, 0, sizeof(DmemInfo), pa);
    q_raw("Q7", "info size 10", pa, 0, 0x10, pa);
    q_raw("Q8", "info size 0", pa, 0, 0, pa);
    q_raw("Q9", "offset = dmem size", (long)g_dmem, 0, sizeof(DmemInfo), pa);
    q_raw("Q10", "find-next from last page", (long)(g_dmem - PG), 1, sizeof(DmemInfo), pa);
    q_raw("Q11", "flags 2", pa, 2, sizeof(DmemInfo), pa);
    q_raw("Q17", "flags 3", pa, 3, sizeof(DmemInfo), pa);
    q_raw("Q18", "flags -1", pa, -1, sizeof(DmemInfo), pa);
    {
        // Bytes written into a 0xCC-filled buffer for each info size.
        static const uint32_t sizes[] = {0, 1, 4, 7, 8, 9, 0xF, 0x10, 0x11, 0x14, 0x18, 0x20, 0x40};
        Line l = head("QS", "info size: bytes written");
        for (uint32_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            unsigned char buf[0x48];
            my_memset(buf, 0xCC, sizeof(buf));
            const int32_t rq = sceKernelDirectMemoryQuery(pa, 0, buf, sizes[i]);
            int last = 0;
            for (int k = 0; k < 0x48; k++)
                if (buf[k] != 0xCC)
                    last = k + 1;
            l.hex(sizes[i]).c(':');
            if (rq != 0)
                l.s("e");
            else
                l.hex((uint64_t)last);
            l.c(' ');
        }
        l.end();
    }
    release_all(pa, 2);
}

// ---- V: available direct memory --------------------------------------------------------------

static void v_line(const char* id, const char* name, long lo, long hi, size_t align, long ref) {
    long phys = -1;
    size_t size = 0;
    const int32_t r = sceKernelAvailableDirectMemorySize(lo, hi, align, &phys, &size);
    Line l = head(id, name);
    l.ret("ret", r).s("phys=").shex((long long)(phys - ref)).s(" size=").hex(size).end();
}

static void run_v() {
    say("== V  available size; layout A(0-3) hole(4-7) B(8-11) free(12-19), rel to base");
    const int t[5] = {3, -1, 3, -1, -1};
    long blk[5];
    const long c = build(5, t, blk, 0x40000);
    if (c < 0)
        return say("V1   SKIP layout");
    v_line("V1", "whole layout, align 4000", c, c + (long)(20 * PG), PG, c);
    v_line("V2", "whole layout, align 20000", c, c + (long)(20 * PG), 0x20000, c);
    v_line("V3", "only the hole 4-7", c + (long)(4 * PG), c + (long)(8 * PG), PG, c);
    v_line("V4", "only allocated A", c, c + (long)(4 * PG), PG, c);
    v_line("V5", "end before start", c + (long)(8 * PG), c, PG, c);
    v_line("V6", "align 3000", c, c + (long)(20 * PG), 0x3000, c);
    for (int i = 0; i < 5; i++)
        release_all(blk[i], 4);
}

// ---- E: allocate / map validation ------------------------------------------------------------

static void e_alloc(const char* id, const char* name, long lo, long hi, size_t len, size_t align,
                    int type) {
    long pa = -1;
    const int32_t r = sceKernelAllocateDirectMemory(lo, hi, len, align, type, &pa);
    Line l = head(id, name);
    l.ret("ret", r);
    if (r == 0) {
        l.s("phys%4000=").hex((uint64_t)pa % PG);
        sceKernelReleaseDirectMemory(pa - (pa % (long)PG), len ? (len + PG - 1) / PG * PG : PG);
    }
    l.end();
}

static void e_map(const char* id, const char* name, long phys, size_t len, int prot) {
    void* va = 0;
    const int32_t r = sceKernelMapDirectMemory(&va, len, prot, 0, phys, PG);
    Line l = head(id, name);
    l.ret("ret", r);
    if (r == 0) {
        int32_t pr = -1;
        l.s("prot=").c(mapped(va, &pr) ? (char)('0' + (pr & 7)) : '.');
        if (len)
            sceKernelMunmap(va, len);
    }
    l.end();
}

static void run_e() {
    say("== E  allocate / map validation");
    e_alloc("E1", "alloc len 0", 0, (long)g_dmem, 0, PG, 3);
    e_alloc("E2", "alloc len 1000", 0, (long)g_dmem, 0x1000, PG, 3);
    e_alloc("E3", "alloc align 3000", 0, (long)g_dmem, PG, 0x3000, 3);
    e_alloc("E4", "alloc align 1000", 0, (long)g_dmem, PG, 0x1000, 3);
    e_alloc("E5", "alloc align 0", 0, (long)g_dmem, PG, 0, 3);
    e_alloc("E6", "alloc type 11", 0, (long)g_dmem, PG, PG, 11);
    e_alloc("E7", "alloc type -1", 0, (long)g_dmem, PG, PG, -1);
    e_alloc("E8", "alloc range smaller than len", (long)(g_dmem - PG), (long)g_dmem, 2 * PG, PG,
            3);
    e_alloc("E9", "alloc search start +1000", 0x1000, (long)g_dmem, PG, PG, 3);
    e_alloc("E10", "alloc end before start", (long)g_dmem, 0, PG, PG, 3);
    int32_t r = 0;
    const long pa = alloc(4, 3, &r);
    if (pa < 0)
        return setup_failed("E11", "map validation", "alloc", r);
    sceKernelReleaseDirectMemory(pa + (long)(2 * PG), 2 * PG);
    e_map("E11", "map free page", pa + (long)(2 * PG), PG, 0x3);
    e_map("E12", "map phys +1000", pa + 0x1000, PG, 0x3);
    e_map("E13", "map len 5000", pa, PG + 0x1000, 0x3);
    e_map("E14", "map half free (2 alloc + 2 free)", pa, 4 * PG, 0x3);
    e_map("E15", "map len 0", pa, 0, 0x3);
    e_map("E16", "map prot 0", pa, PG, 0);
    e_map("E17", "map prot 1 (read only)", pa, PG, 0x1);
    release_all(pa, 4);
}

// ---- F: mapping variants -------------------------------------------------------------------

static Line& prot_str(Line& l, uint8_t* va, int pages) {
    l.s("prot=[");
    for (int i = 0; i < pages; i++) {
        int32_t pr = 0;
        l.c(mapped(va + i * PG, &pr) ? (char)('0' + (pr & 7)) : '.');
    }
    return l.s("]");
}

static uint8_t* f_setup(const char* id, const char* name, long* pa, int pages, uint32_t tag) {
    int32_t r = 0;
    *pa = alloc(pages, 3, &r);
    if (*pa < 0) {
        setup_failed(id, name, "alloc", r);
        return nullptr;
    }
    uint8_t* va = map(*pa, pages, &r);
    if (!va) {
        setup_failed(id, name, "map", r);
        release_all(*pa, pages);
        return nullptr;
    }
    stamp(va, pages, tag);
    return va;
}

static void run_f() {
    say("== F  mapping variants, 4 pages unless noted");
    long pa = -1;
    if (uint8_t* va = f_setup("F1", "mtype 2-3 -> 0, rel 1-2", &pa, 4, 0x5A0F1000)) {
        const int32_t rm = sceKernelMtypeprotect(va + 2 * PG, 2 * PG, 0, 0x3);
        DmemInfo q;
        my_memset(&q, 0, sizeof(q));
        sceKernelDirectMemoryQuery(pa + (long)(2 * PG), 0, &q, sizeof(q));
        Line l = head("F1", "mtype 2-3 -> 0, rel 1-2");
        l.ret("mt", rm).s("q2mtype=").dec(q.mtype).c(' ');
        l.ret("ret", sceKernelReleaseDirectMemory(pa + (long)PG, 2 * PG));
        va_str(l, "va", va, pa, 4, 0x5A0F1000).c(' ');
        phys_str(l, pa, 4).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F2", "mprotect 1-2 ro, rel all", &pa, 4, 0x5A0F2000)) {
        const int32_t rp = sceKernelMprotect(va + PG, 2 * PG, 0x1);
        Line l = head("F2", "mprotect 1-2 ro, rel all");
        l.ret("mp", rp);
        prot_str(l, va, 4).c(' ');
        l.ret("ret", sceKernelReleaseDirectMemory(pa, 4 * PG));
        va_str(l, "va", va, pa, 4, 0x5A0F2000).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F3", "munmap page 1, rel all", &pa, 4, 0x5A0F3000)) {
        const int32_t ru = sceKernelMunmap(va + PG, PG);
        Line l = head("F3", "munmap page 1, rel all");
        l.ret("unmap", ru).ret("ret", sceKernelReleaseDirectMemory(pa, 4 * PG));
        va_str(l, "va", va, pa, 4, 0x5A0F3000).c(' ');
        phys_str(l, pa, 4).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    {
        int32_t r = 0;
        pa = alloc(4, 3, &r);
        uint8_t* v1 = pa >= 0 ? map(pa, 2, &r) : nullptr;
        uint8_t* v2 = pa >= 0 ? map(pa + 2 * PG, 2, &r) : nullptr;
        if (!v1 || !v2) {
            setup_failed("F4", "two maps 0-1 / 2-3, rel 1-2", "alloc/map", r);
        } else {
            stamp(v1, 2, 0x5A0F4000);
            stamp(v2, 2, 0x5A0F4002);
            Line l = head("F4", "two maps 0-1 / 2-3, rel 1-2");
            l.ret("ret", sceKernelReleaseDirectMemory(pa + (long)PG, 2 * PG));
            va_str(l, "va1", v1, pa, 2, 0x5A0F4000).c(' ');
            va_str(l, "va2", v2, pa + 2 * PG, 2, 0x5A0F4002).c(' ');
            phys_str(l, pa, 4).end();
        }
        unmap_all(v1, 2);
        unmap_all(v2, 2);
        release_all(pa, 4);
    }
    {
        int32_t r = 0;
        pa = alloc(4, 3, &r);
        uint8_t* v1 = pa >= 0 ? map(pa, 4, &r) : nullptr;
        uint8_t* v2 = pa >= 0 ? map(pa, 4, &r) : nullptr;
        if (!v1 || !v2) {
            setup_failed("F5", "aliased twice, rel 1-2", "alloc/map", r);
        } else {
            stamp(v1, 4, 0x5A0F5000);
            Line l = head("F5", "aliased twice, rel 1-2");
            l.ret("ret", sceKernelReleaseDirectMemory(pa + (long)PG, 2 * PG));
            va_str(l, "va1", v1, pa, 4, 0x5A0F5000).c(' ');
            va_str(l, "va2", v2, pa, 4, 0x5A0F5000).c(' ');
            phys_str(l, pa, 4).end();
        }
        unmap_all(v1, 4);
        unmap_all(v2, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F6", "rel all, re-alloc same phys", &pa, 4, 0x5A0F6000)) {
        sceKernelReleaseDirectMemory(pa, 4 * PG);
        int32_t r = 0;
        const long again = alloc_in(pa, pa + 4 * PG, 4, 3, &r);
        Line l = head("F6", "rel all, re-alloc same phys");
        l.ret("realloc", r).s("same=").c(again == pa ? '1' : '0').c(' ');
        va_str(l, "oldva", va, pa, 4, 0x5A0F6000).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F7", "rel 1-2, munmap whole range", &pa, 4, 0x5A0F7000)) {
        sceKernelReleaseDirectMemory(pa + (long)PG, 2 * PG);
        Line l = head("F7", "rel 1-2, munmap whole range");
        l.ret("unmap", sceKernelMunmap(va, 4 * PG));
        va_str(l, "va", va, pa, 4, 0x5A0F7000).c(' ');
        phys_str(l, pa, 4).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F8", "rel 1-2, mprotect released 1", &pa, 4, 0x5A0F8000)) {
        sceKernelReleaseDirectMemory(pa + (long)PG, 2 * PG);
        Line l = head("F8", "rel 1-2, mprotect released 1");
        l.ret("mp", sceKernelMprotect(va + PG, PG, 0x1));
        prot_str(l, va, 4).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
    if (uint8_t* va = f_setup("F9", "mtype released page 1", &pa, 4, 0x5A0F9000)) {
        sceKernelReleaseDirectMemory(pa + (long)PG, PG);
        Line l = head("F9", "mtype released page 1");
        l.ret("mt", sceKernelMtypeprotect(va + PG, PG, 0, 0x3));
        va_str(l, "va", va, pa, 4, 0x5A0F9000).end();
        unmap_all(va, 4);
        release_all(pa, 4);
    }
}

// ---- P: memory pool --------------------------------------------------------------------------

static void run_p() {
    say("== P  memory pool, 64 KiB blocks");
    const uint64_t blk = 0x10000;
    uint64_t pp = 0;
    const int32_t re = sceKernelMemoryPoolExpand(0, g_dmem, blk, blk, &pp);
    {
        Line l = head("P1", "pool expand 64K");
        l.ret("ret", re).end();
    }
    if (re == 0) {
        q_raw("P2", "query pooled block", (long)pp, 0, sizeof(DmemInfo), (long)pp);
        q_raw("P3", "find-next from pooled block", (long)pp, 1, sizeof(DmemInfo), (long)pp);
    }
    void* va = 0;
    const int32_t rr = sceKernelMemoryPoolReserve(0, blk, blk, 0, &va);
    const int32_t rc = rr == 0 ? sceKernelMemoryPoolCommit(va, blk, 3, 0x3, 0) : -1;
    {
        Line l = head("P4", "reserve + commit 64K");
        l.ret("res", rr).ret("commit", rc);
        if (rc == 0) {
            int32_t pr = 0;
            l.s("prot=").c(mapped(va, &pr) ? (char)('0' + (pr & 7)) : '.');
            if (mapped(va, &pr) && (pr & 1)) {
                *(volatile uint32_t*)va = 0x5A0B0001;
                l.s(" rw=").c(*(volatile uint32_t*)va == 0x5A0B0001 ? '1' : '0');
            }
        }
        l.end();
    }
    if (rc == 0) {
        const int32_t rd = sceKernelMemoryPoolDecommit(va, blk, 0);
        int32_t pr = 0;
        Line l = head("P5", "decommit 64K");
        l.ret("ret", rd).s("va=").c(mapped(va, &pr) ? (char)('0' + (pr & 7)) : '.').end();
    }
    if (re == 0) {
        Line l = head("P6", "rel  pooled block");
        l.ret("ret", sceKernelReleaseDirectMemory((long)pp, blk));
        l.end();
        q_raw("P7", "query pooled block after rel", (long)pp, 0, sizeof(DmemInfo), (long)pp);
    }
    uint64_t pp2 = 0;
    const int32_t re2 = sceKernelMemoryPoolExpand(0, g_dmem, blk, blk, &pp2);
    if (re2 == 0) {
        Line l = head("P8", "chk  pooled block");
        l.ret("ret", sceKernelCheckedReleaseDirectMemory((long)pp2, blk));
        l.end();
        q_raw("P9", "query after chk", (long)pp2, 0, sizeof(DmemInfo), (long)pp2);
    } else {
        Line l = head("P8", "pool expand again");
        l.ret("ret", re2).end();
    }
}

// ---- G: release under a large mapping ----------------------------------------------------------

static void g_case(const char* id, const char* name, int prot) {
    const int pages = 0x400;   // 16 MiB, like the framebuffer
    int32_t r = 0;
    const long pa = alloc_al(0, g_dmem, pages, 0x200000, 3, &r);
    if (pa < 0)
        return setup_failed(id, name, "alloc", r);
    void* v = 0;
    r = sceKernelMapDirectMemory(&v, pages * PG, prot, 0, pa, 0x200000);
    uint8_t* va = r == 0 ? (uint8_t*)v : nullptr;
    if (!va) {
        setup_failed(id, name, "map", r);
        return release_all(pa, pages);
    }
    stamp(va, pages, 0x5A060000);
    {
        Line l;
        l.s(id).s("   starting").end();
    }
    const int32_t rr = sceKernelReleaseDirectMemory(pa, pages * PG);
    int maps = 0, allocs = 0;
    for (int i = 0; i < pages; i++) {
        maps += mapped(va + i * PG, nullptr) ? 1 : 0;
        allocs += allocated(pa + i * PG) ? 1 : 0;
    }
    Line l = head(id, name);
    l.ret("ret", rr).s("mapped=").hex((uint64_t)maps).s(" allocated=").hex((uint64_t)allocs).end();
    unmap_all(va, pages);
    release_all(pa, pages);
}

static void run_g() {
    say("== G  release all of a 16 MiB 2 MiB-aligned mapped block (pages 400)");
    g_case("G1", "rel  16M, prot 3", 0x3);
    g_case("G2", "rel  16M, prot 33 (GPU)", 0x33);
}

// ---- H: ladders over values that were tested at one point only -------------------------------

static char code(int32_t r) {
    switch ((uint32_t)r) {
    case 0:
        return 'o';
    case 0x80020016:
        return 'I';
    case 0x8002000C:
        return 'M';
    case 0x8002000D:
        return 'A';
    case 0x80020002:
        return 'N';
    default:
        return '?';
    }
}

static void run_h() {
    say("== H  ladders; codes: o ok, I EINVAL, M ENOMEM, A EACCES, N ENOENT, ? other");
    static const uint64_t starts[] = {1ull << 33, 1ull << 40, 1ull << 62, 0x7FFFFFFFFFFFC000ull,
                                      0x8000000000000000ull};
    {
        Line l = head("H1", "start: rel/chk (1<<33,40,62,7F..,63)");
        for (uint64_t st : starts) {
            l.c(code(sceKernelReleaseDirectMemory((long)st, PG)));
            l.c(code(sceKernelCheckedReleaseDirectMemory((long)st, PG))).c(' ');
        }
        l.end();
    }
    int32_t r = 0;
    const long pa = alloc(2, 3, &r);
    if (pa < 0)
        return setup_failed("H2", "ladders", "alloc", r);
    {
        Line l = head("H2", "query flags 1<<31 .. 1<<0");
        for (int b = 31; b >= 0; b--) {
            DmemInfo q;
            l.c(code(sceKernelDirectMemoryQuery(pa, (int32_t)(1u << b), &q, sizeof(q))));
        }
        l.end();
    }
    {
        Line l = head("H3", "alloc memory type -2 .. 12");
        for (int t = -2; t <= 12; t++) {
            long ph = -1;
            const int32_t ra = sceKernelAllocateDirectMemory(0, (long)g_dmem, PG, PG, t, &ph);
            l.c(code(ra));
            if (ra == 0)
                sceKernelReleaseDirectMemory(ph, PG);
        }
        l.end();
    }
    {
        static const uint64_t al[] = {0,      0x1000,   0x2000,     0x3000,     0x4000,   0x6000,
                                      0x8000, 0x10000,  0x200000,   1ull << 31, 1ull << 32,
                                      1ull << 40};
        Line l = head("H4", "available align 0,1000..1<<40");
        for (uint64_t a1 : al) {
            long ph = -1;
            size_t sz = 0;
            l.c(code(sceKernelAvailableDirectMemorySize(0, (long)g_dmem, a1, &ph, &sz)));
        }
        l.end();
    }
    {
        Line l = head("H5", "available, null outputs");
        l.ret("ret", sceKernelAvailableDirectMemorySize(0, (long)g_dmem, PG, nullptr, nullptr));
        l.end();
    }
    {
        // bytes written by a failing query, and by find-next past the end
        unsigned char buf[0x48];
        Line l = head("H6", "bytes written by failing query");
        for (int pass = 0; pass < 2; pass++) {
            my_memset(buf, 0xCC, sizeof(buf));
            const long at = pass == 0 ? pa + (long)(2 * PG) : (long)(g_dmem - PG);
            const int32_t rq = sceKernelDirectMemoryQuery(at, pass, buf, 0x18);
            int last = 0;
            for (int k = 0; k < 0x48; k++)
                if (buf[k] != 0xCC)
                    last = k + 1;
            l.c(code(rq)).c(':').hex((uint64_t)last).c(' ');
        }
        l.end();
    }
    release_all(pa, 2);
}

// ---- H7: checked release beyond dmem, boundary found on the console ----------------------------

static int32_t chk(uint64_t start, uint64_t len) {
    return sceKernelCheckedReleaseDirectMemory((long)start, len);
}

static void run_h7() {
    say("== H7 checked release past dmem end: ENOENT below X, OK from X; start in [1<<33, 1<<40]");
    const uint64_t lo0 = 1ull << 33, hi0 = 1ull << 40;
    {
        Line l = head("H7", "16 samples 1<<33..1<<40 chk/rel");
        for (int i = 0; i < 16; i++)
            l.c(code(chk((lo0 + (hi0 - lo0) / 15 * i) & ~(PG - 1), PG)));
        l.c(' ');
        for (int i = 0; i < 16; i++)
            l.c(code(sceKernelReleaseDirectMemory(
                (long)((lo0 + (hi0 - lo0) / 15 * i) & ~(PG - 1)), PG)));
        l.end();
    }
    if (chk(lo0, PG) == 0 || chk(hi0, PG) != 0)
        return say("H8   SKIP not bracketed");
    uint64_t lo = lo0, hi = hi0;
    while (hi - lo > PG) {
        uint64_t mid = (lo + (hi - lo) / 2) & ~(PG - 1);
        if (mid <= lo)
            mid = lo + PG;
        if (chk(mid, PG) == 0)
            hi = mid;
        else
            lo = mid;
    }
    {
        Line l = head("H8", "boundary X");
        l.s("X=").hex(hi).s(" X-4000:").c(code(chk(hi - PG, PG))).s(" X:").c(code(chk(hi, PG)));
        l.s(" X+4000:").c(code(chk(hi + PG, PG))).s(" X-4000,len8000:");
        l.c(code(chk(hi - PG, 2 * PG))).s(" 1<<33,len to X+4000:");
        l.c(code(chk(lo0, hi + PG - lo0))).end();
    }
}

// ---- P2: memory pool with a 2 MiB reservation ---------------------------------------------------

struct PoolStats {
    int32_t avail_flushed, avail_cached, alloc_flushed, alloc_cached;
};

static Line& stats(Line& l) {
    PoolStats st;
    my_memset(&st, 0, sizeof(st));
    const int32_t r = sceKernelMemoryPoolGetBlockStats(&st, sizeof(st));
    l.s("stats=");
    if (r != 0)
        return l.s("err");
    return l.hex((uint32_t)st.avail_flushed).c('/').hex((uint32_t)st.avail_cached).c('/')
        .hex((uint32_t)st.alloc_flushed).c('/').hex((uint32_t)st.alloc_cached);
}

static void run_p2() {
    say("== P2 pool with 2 MiB reserve; stats = avail flushed/cached, alloc flushed/cached");
    const uint64_t blk = 0x10000;
    {
        Line l = head("P20", "stats before");
        stats(l).end();
    }
    // [pool block 0-3][hole 4-7][B 8-11], 64 KiB aligned
    int32_t r = 0;
    const long c = alloc_al(0, g_dmem, 12, blk, 3, &r);
    if (c < 0)
        return setup_failed("P21", "pool layout", "alloc", r);
    sceKernelReleaseDirectMemory(c, 12 * PG);
    uint64_t pp = 0;
    const int32_t re = sceKernelMemoryPoolExpand((uint64_t)c, (uint64_t)c + blk, blk, blk, &pp);
    const long b = alloc_in(c + 8 * PG, c + 12 * PG, 4, 3, &r);
    {
        Line l = head("P21", "expand at base, B at +20000");
        l.ret("ret", re).s("at_base=").c(re == 0 && pp == (uint64_t)c ? '1' : '0');
        l.s(" B=").c(b == (long)(c + 8 * PG) ? '1' : '0').c(' ');
        stats(l).end();
    }
    if (re == 0 && pp == (uint64_t)c && b == (long)(c + 8 * PG)) {
        q_raw("P22", "find-next at pool end (hole start)", c + (long)(4 * PG), 1,
              sizeof(DmemInfo), c);
        q_raw("P23", "find-next inside hole", c + (long)(5 * PG), 1, sizeof(DmemInfo), c);
        e_map("P24", "map pooled block", c, blk, 0x3);
    }
    {
        // search window smaller than the length
        uint64_t px = 0;
        const uint64_t w = (uint64_t)c + 0x40000;
        const int32_t rx = sceKernelMemoryPoolExpand(w, w + 0x4000, blk, blk, &px);
        Line l = head("P25", "expand, window 4000 < len 10000");
        l.ret("ret", rx);
        if (rx == 0)
            l.s("inside=").c(px >= w && px + blk <= w + 0x4000 ? '1' : '0');
        l.end();
    }
    void* va = 0;
    const int32_t rr = sceKernelMemoryPoolReserve(0, 0x200000, 0, 0, &va);
    int32_t rc3 = -1, rc0 = -1;
    if (rr == 0) {
        rc3 = sceKernelMemoryPoolCommit(va, blk, 3, 0x3, 0);
        if (rc3 != 0)
            rc0 = sceKernelMemoryPoolCommit(va, blk, 0, 0x3, 0);
    }
    const bool committed = rc3 == 0 || rc0 == 0;
    {
        Line l = head("P26", "reserve 2M, commit 64K");
        l.ret("res", rr).ret("c3", rc3).ret("c0", rc0);
        if (committed) {
            *(volatile uint32_t*)va = 0x5A0C0001;
            l.s("rw=").c(*(volatile uint32_t*)va == 0x5A0C0001 ? '1' : '0').c(' ');
        }
        stats(l).end();
    }
    if (committed && re == 0 && pp == (uint64_t)c) {
        q_raw("P27", "exact query of pool block", c, 0, sizeof(DmemInfo), c);
        {
            Line l = head("P28", "chk  pool block, committed");
            l.ret("ret", sceKernelCheckedReleaseDirectMemory(c, blk));
            int32_t pr = 0;
            l.s("va=").c(mapped(va, &pr) ? 'M' : '.').c(' ');
            stats(l).end();
        }
        {
            Line l = head("P29", "rel  pool block, committed");
            l.ret("ret", sceKernelReleaseDirectMemory(c, blk));
            int32_t pr = 0;
            l.s("va=").c(mapped(va, &pr) ? 'M' : '.').c(' ');
            stats(l).end();
        }
    }
    if (committed) {
        Line l = head("P30", "decommit 64K");
        l.ret("ret", sceKernelMemoryPoolDecommit(va, blk, 0));
        int32_t pr = 0;
        l.s("va=").c(mapped(va, &pr) ? 'M' : '.').c(' ');
        stats(l).end();
    }
    if (re == 0) {
        Line l = head("P31", "rel  pool block, decommitted");
        l.ret("ret", sceKernelReleaseDirectMemory((long)pp, blk)).c(' ');
        stats(l).end();
    }
    release_all(b, 4);
}

// ---- P4: pool block counters through commit, decommit and unmap ---------------------------------

static void p_line(const char* id, const char* name, const char* tag, int32_t r) {
    Line l = head(id, name);
    l.ret(tag, r);
    stats(l).end();
}

static void run_p4() {
    say("== P4 pool counters; stats = avail flushed/cached, alloc flushed/cached");
    const uint64_t blk = 0x10000, rsv = 0x200000;
    {
        Line l = head("P40", "stats before");
        stats(l).end();
    }
    uint64_t px = 0;
    p_line("P41", "expand 4 blocks", "ret", sceKernelMemoryPoolExpand(0, g_dmem, 4 * blk, blk, &px));
    void* r1 = 0;
    void* r2 = 0;
    const int32_t a1 = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &r1);
    const int32_t a2 = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &r2);
    {
        Line l = head("P42", "reserve R1, R2 (2M each)");
        l.ret("r1", a1).ret("r2", a2);
        stats(l).end();
    }
    if (a1 != 0 || a2 != 0)
        return say("P43  SKIP reserve");
    uint8_t* b1 = (uint8_t*)r1;
    uint8_t* b2 = (uint8_t*)r2;
    p_line("P43", "commit R1+0 64K t3", "ret", sceKernelMemoryPoolCommit(b1, blk, 3, 0x3, 0));
    p_line("P44", "commit R1+10000 64K t3", "ret",
           sceKernelMemoryPoolCommit(b1 + blk, blk, 3, 0x3, 0));
    p_line("P45", "commit R2+0 64K t0", "ret", sceKernelMemoryPoolCommit(b2, blk, 0, 0x3, 0));
    p_line("P46", "commit R1+20000 128K t3", "ret",
           sceKernelMemoryPoolCommit(b1 + 2 * blk, 2 * blk, 3, 0x3, 0));
    p_line("P47", "decommit R1+10000 64K", "ret", sceKernelMemoryPoolDecommit(b1 + blk, blk, 0));
    p_line("P48", "decommit R1+0 64K", "ret", sceKernelMemoryPoolDecommit(b1, blk, 0));
    p_line("P49", "decommit R1+20000 128K", "ret",
           sceKernelMemoryPoolDecommit(b1 + 2 * blk, 2 * blk, 0));
    p_line("P50", "munmap R1 (all decommitted)", "ret", sceKernelMunmap(b1, rsv));
    void* r3 = 0;
    const int32_t a3 = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &r3);
    const int32_t c3 = a3 == 0 ? sceKernelMemoryPoolCommit(r3, blk, 3, 0x3, 0) : -1;
    {
        Line l = head("P51", "reserve R3, commit 64K t3");
        l.ret("res", a3).ret("commit", c3);
        stats(l).end();
    }
    {
        const int32_t ru = sceKernelMunmap(b2, rsv);
        int32_t pr = 0;
        Line l = head("P52", "munmap R2, 64K still committed");
        l.ret("ret", ru).s("va=").c(mapped(b2, &pr) ? 'M' : '.').c(' ');
        stats(l).end();
    }
    if (a3 == 0) {
        const int32_t ru = sceKernelMunmap(r3, rsv);
        int32_t pr = 0;
        Line l = head("P53", "munmap R3, 64K still committed");
        l.ret("ret", ru).s("va=").c(mapped(r3, &pr) ? 'M' : '.').c(' ');
        stats(l).end();
    }
}

// ---- P7: pool model; every line prints counter deltas ------------------------------------------

struct PS {
    int32_t af, ac, lf, lc;
};

static PS ps() {
    PS st;
    my_memset(&st, 0, sizeof(st));
    sceKernelMemoryPoolGetBlockStats(&st, sizeof(st));
    return st;
}

// Which counter moved: one char for (af, ac, lf, lc) going down (-) / up (+) / not at all.
static char moved(const PS& a, const PS& b) {
    const int d[4] = {b.af - a.af, b.ac - a.ac, b.lf - a.lf, b.lc - a.lc};
    // encode the pair "from where -> to where" as a single letter for one-block moves
    if (d[0] == -1 && d[2] == 1 && !d[1] && !d[3]) return 'F';  // avail flushed -> alloc flushed
    if (d[0] == -1 && d[3] == 1 && !d[1] && !d[2]) return 'f';  // avail flushed -> alloc cached
    if (d[1] == -1 && d[3] == 1 && !d[0] && !d[2]) return 'C';  // avail cached  -> alloc cached
    if (d[1] == -1 && d[2] == 1 && !d[0] && !d[3]) return 'c';  // avail cached  -> alloc flushed
    if (d[2] == -1 && d[0] == 1 && !d[1] && !d[3]) return 'R';  // alloc flushed -> avail flushed
    if (d[2] == -1 && d[1] == 1 && !d[0] && !d[3]) return 'r';  // alloc flushed -> avail cached
    if (d[3] == -1 && d[1] == 1 && !d[0] && !d[2]) return 'K';  // alloc cached  -> avail cached
    if (d[3] == -1 && d[0] == 1 && !d[1] && !d[2]) return 'k';  // alloc cached  -> avail flushed
    if (!d[0] && !d[1] && !d[2] && !d[3]) return '=';
    return '*';
}

static Line& delta(Line& l, const PS& a, const PS& b) {
    const int d[4] = {b.af - a.af, b.ac - a.ac, b.lf - a.lf, b.lc - a.lc};
    l.s("d=");
    for (int i = 0; i < 4; i++) {
        if (i)
            l.c('/');
        l.dec(d[i]);
    }
    return l;
}

static void run_p7() {
    say("== P7 pool model. moves: F af>lf  f af>lc  C ac>lc  c ac>lf  R lf>af  r lf>ac  K lc>ac  "
        "k lc>af  = none  * other");
    const uint64_t blk = 0x10000, rsv = 0x200000;
    uint64_t px = 0;
    {
        const PS a = ps();
        const int32_t r = sceKernelMemoryPoolExpand(0, g_dmem, 16 * blk, blk, &px);
        Line l = head("P70", "expand 16 blocks");
        l.ret("ret", r);
        delta(l, a, ps()).end();
    }
    void* rv = 0;
    PS a = ps();
    const int32_t rr = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &rv);
    {
        Line l = head("P70b", "reserve 2M");
        l.ret("ret", rr).s("move=").c(moved(a, ps())).end();
    }
    if (rr != 0)
        return;
    uint8_t* base = (uint8_t*)rv;
    int32_t cr[11];
    {
        Line l = head("P71", "commit 64K type 0..10: move");
        for (int t = 0; t <= 10; t++) {
            a = ps();
            cr[t] = sceKernelMemoryPoolCommit(base + t * blk, blk, t, 0x3, 0);
            l.c(cr[t] == 0 ? moved(a, ps()) : code(cr[t]));
        }
        l.end();
    }
    {
        Line l = head("P72", "decommit each: move");
        for (int t = 0; t <= 10; t++) {
            if (cr[t] != 0) {
                l.c('-');
                continue;
            }
            a = ps();
            const int32_t rd = sceKernelMemoryPoolDecommit(base + t * blk, blk, 0);
            l.c(rd == 0 ? moved(a, ps()) : code(rd));
        }
        l.end();
    }
    sceKernelMunmap(rv, rsv);
    {
        Line l = head("P73", "reserve 2,4,6,8M / munmap: delta");
        for (int m = 1; m <= 4; m++) {
            void* v = 0;
            a = ps();
            const int32_t r = sceKernelMemoryPoolReserve(0, m * rsv, 0, 0, &v);
            l.c(code(r)).c(':');
            delta(l, a, ps());
            if (r == 0) {
                a = ps();
                sceKernelMunmap(v, m * rsv);
                l.c('|');
                delta(l, a, ps());
            }
            l.c(' ');
        }
        l.end();
    }
    {
        // both available pools non-empty: from which does each kind draw?
        const PS st = ps();
        Line l = head("P74", "draw order (needs af>0 and ac>0)");
        l.s("af=").dec(st.af).s(" ac=").dec(st.ac).c(' ');
        if (st.af > 0 && st.ac > 0) {
            void* v = 0;
            if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) == 0) {
                uint8_t* b = (uint8_t*)v;
                a = ps();
                const int32_t c3 = sceKernelMemoryPoolCommit(b, blk, 3, 0x3, 0);
                l.s("t3:").c(c3 == 0 ? moved(a, ps()) : code(c3));
                if (c3 == 0)
                    sceKernelMemoryPoolDecommit(b, blk, 0);
                a = ps();
                const int32_t c0 = sceKernelMemoryPoolCommit(b, blk, 0, 0x3, 0);
                l.s(" t0:").c(c0 == 0 ? moved(a, ps()) : code(c0));
                if (c0 == 0)
                    sceKernelMemoryPoolDecommit(b, blk, 0);
                sceKernelMunmap(v, rsv);
            }
            void* w = 0;
            a = ps();
            const int32_t r2 = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &w);
            l.s(" reserve:").c(r2 == 0 ? moved(a, ps()) : code(r2));
            if (r2 == 0)
                sceKernelMunmap(w, rsv);
        }
        l.end();
    }
    {
        // drain every available block into 64K commits, then try a reserve and a commit
        void* rs[16];
        int nr = 0, used = 32, commits = 0;
        int32_t last = 0;
        for (int it = 0; it < 512; it++) {
            const PS st = ps();
            if (st.af + st.ac == 0)
                break;
            if (used == 32) {
                if (nr == 16 || sceKernelMemoryPoolReserve(0, rsv, 0, 0, &rs[nr]) != 0)
                    break;
                nr++;
                used = 0;
                continue;
            }
            last = sceKernelMemoryPoolCommit((uint8_t*)rs[nr - 1] + used * blk, blk, 3, 0x3, 0);
            if (last != 0)
                break;
            used++;
            commits++;
        }
        const PS st = ps();
        void* v = 0;
        const int32_t r_res = sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v);
        if (r_res == 0)
            sceKernelMunmap(v, rsv);
        int32_t r_com = -1;
        if (nr > 0 && used < 32)
            r_com = sceKernelMemoryPoolCommit((uint8_t*)rs[nr - 1] + used * blk, blk, 3, 0x3, 0);
        Line l = head("P75", "drained pool: reserve / commit");
        l.s("commits=").dec(commits).s(" reserves=").dec(nr).s(" left=").dec(st.af + st.ac);
        l.s(" last=").c(code(last)).s(" reserve=").c(code(r_res));
        l.s(" commit=").c(r_com == -1 ? '-' : code(r_com));
        if (r_com == 0)
            used++;
        for (int i = 0; i < nr; i++)
            sceKernelMunmap(rs[i], rsv);
        l.c(' ');
        stats(l).end();
    }
}

// ---- P8: reservation sizes, partial unmap, split draws, timing of returned blocks ---------------

static Line& d4(Line& l, const PS& a, const PS& b) {
    l.dec(b.af - a.af).c('/').dec(b.ac - a.ac).c('/').dec(b.lf - a.lf).c('/').dec(b.lc - a.lc);
    return l;
}

static Line& s4(Line& l, const PS& a) {
    l.dec(a.af).c('/').dec(a.ac).c('/').dec(a.lf).c('/').dec(a.lc);
    return l;
}

static void reserve_sizes(const char* id, const char* name, const int* mb, int n) {
    Line l = head(id, name);
    for (int i = 0; i < n; i++) {
        void* v = 0;
        const PS a = ps();
        const int32_t r = sceKernelMemoryPoolReserve(0, (uint64_t)mb[i] << 20, 0, 0, &v);
        l.c(code(r)).c(':');
        d4(l, a, ps());
        if (r == 0) {
            const PS b = ps();
            sceKernelMunmap(v, (uint64_t)mb[i] << 20);
            l.c('|');
            d4(l, b, ps());
        }
        l.c(' ');
    }
    l.end();
}

static void series(Line& l) {
    static const unsigned us[] = {0, 1000, 9000, 90000, 900000};
    const char* tag[] = {"0:", " 1ms:", " 10ms:", " 100ms:", " 1s:"};
    for (int i = 0; i < 5; i++) {
        if (us[i])
            sceKernelUsleep(us[i]);
        l.s(tag[i]);
        s4(l, ps());
    }
}

static void run_p8() {
    say("== P8 pool: stats are af/ac/lf/lc, deltas the same order");
    const uint64_t blk = 0x10000, mb = 0x100000;
    {
        Line l = head("P80", "stats now / after 1s");
        s4(l, ps());
        sceKernelUsleep(1000000);
        l.s(" -> ");
        s4(l, ps()).end();
    }
    uint64_t px = 0;
    sceKernelMemoryPoolExpand(0, g_dmem, 48 * blk, blk, &px);
    static const int big1[] = {16, 32, 64};
    static const int big2[] = {128, 256, 512};
    reserve_sizes("P81", "reserve 16,32,64M | munmap", big1, 3);
    reserve_sizes("P82", "reserve 128,256,512M | munmap", big2, 3);
    {
        void* v = 0;
        PS a = ps();
        const int32_t r = sceKernelMemoryPoolReserve(0, 4 * mb, 0, 0, &v);
        Line l = head("P83", "4M: reserve | unmap 1st 2M | 2nd");
        l.c(code(r)).c(':');
        d4(l, a, ps());
        if (r == 0) {
            a = ps();
            l.s(" |").c(code(sceKernelMunmap(v, 2 * mb))).c(':');
            d4(l, a, ps());
            a = ps();
            l.s(" |").c(code(sceKernelMunmap((uint8_t*)v + 2 * mb, 2 * mb))).c(':');
            d4(l, a, ps());
        }
        l.end();
    }
    {
        // avail flushed down to exactly 1 with avail cached >= 1, then one 128K type-3 commit
        void* v = 0;
        Line l = head("P84", "128K t3 with af=1, ac>=1");
        if (sceKernelMemoryPoolReserve(0, 8 * mb, 0, 0, &v) != 0) {
            l.s("SKIP reserve").end();
        } else {
            uint8_t* b = (uint8_t*)v;
            int used = 0;
            if (ps().ac == 0) {
                sceKernelMemoryPoolCommit(b, blk, 0, 0x3, 0);
                sceKernelMemoryPoolDecommit(b, blk, 0);
            }
            while (ps().af > 1 && used < 120 &&
                   sceKernelMemoryPoolCommit(b + used * blk, blk, 3, 0x3, 0) == 0)
                used++;
            const PS a = ps();
            l.s("before=");
            s4(l, a);
            if (a.af == 1 && a.ac >= 1) {
                const int32_t r = sceKernelMemoryPoolCommit(b + used * blk, 2 * blk, 3, 0x3, 0);
                l.s(" ret=").c(code(r)).s(" d=");
                d4(l, a, ps());
            } else {
                l.s(" SKIP state");
            }
            sceKernelMunmap(v, 8 * mb);
            sceKernelUsleep(1000000);
            l.end();
        }
    }
    {
        void* v = 0;
        Line l = head("P85", "16x64K t3, munmap: series");
        if (sceKernelMemoryPoolReserve(0, 2 * mb, 0, 0, &v) == 0) {
            int n = 0;
            for (int i = 0; i < 16; i++)
                n += sceKernelMemoryPoolCommit((uint8_t*)v + i * blk, blk, 3, 0x3, 0) == 0;
            l.s("n=").dec(n).s(" before=");
            s4(l, ps()).c(' ');
            sceKernelMunmap(v, 2 * mb);
            series(l);
        } else {
            l.s("SKIP reserve");
        }
        l.end();
    }
    {
        void* v = 0;
        Line l = head("P86", "1M t3 commit, decommit: series");
        if (sceKernelMemoryPoolReserve(0, 2 * mb, 0, 0, &v) == 0) {
            const int32_t r = sceKernelMemoryPoolCommit(v, mb, 3, 0x3, 0);
            l.s("c=").c(code(r)).s(" before=");
            s4(l, ps()).c(' ');
            if (r == 0)
                l.s("dc=").c(code(sceKernelMemoryPoolDecommit(v, mb, 0))).c(' ');
            series(l);
            sceKernelMunmap(v, 2 * mb);
        } else {
            l.s("SKIP reserve");
        }
        l.end();
    }
}

// ---- P9: memory type changed between commit and decommit; GetDirectMemoryType --------------

static void p9_case(const char* id, const char* name, int commit_type, int new_type) {
    const uint64_t blk = 0x10000;
    void* v = 0;
    Line l = head(id, name);
    if (sceKernelMemoryPoolReserve(0, 0x200000, 0, 0, &v) != 0)
        return l.s("SKIP reserve").end();
    PS a = ps();
    const int32_t rc = sceKernelMemoryPoolCommit(v, blk, commit_type, 0x3, 0);
    l.s("commit=").c(code(rc)).c(':');
    d4(l, a, ps());
    if (rc == 0) {
        a = ps();
        l.s(" mtype=").c(code(sceKernelMtypeprotect(v, blk, new_type, 0x3))).c(':');
        d4(l, a, ps());
        a = ps();
        l.s(" decommit=").c(code(sceKernelMemoryPoolDecommit(v, blk, 0))).c(':');
        d4(l, a, ps());
    }
    sceKernelMunmap(v, 0x200000);
    l.end();
}

static void gdt_line(const char* id, const char* name, long at, long ref) {
    int32_t type = -1;
    long st = -1, en = -1;
    const int32_t r = sceKernelGetDirectMemoryType(at, &type, &st, &en);
    Line l = head(id, name);
    l.ret("ret", r);
    if (r == 0)
        l.s("type=").dec(type).s(" start=").shex(st - ref).s(" end=").shex(en - ref);
    l.end();
}

static void run_p9() {
    say("== P9 mtypeprotect between commit and decommit (deltas af/ac/lf/lc); GetDirectMemoryType");
    p9_case("P90", "commit t3, mtype->0, decommit", 3, 0);
    p9_case("P91", "commit t0, mtype->3, decommit", 0, 3);
    uint64_t pp = 0;
    if (sceKernelMemoryPoolExpand(0, g_dmem, 0x10000, 0x10000, &pp) == 0)
        gdt_line("P92", "type of an uncommitted pool block", (long)pp, (long)pp);
    int32_t r = 0;
    const long pa = alloc(2, 3, &r);
    if (pa >= 0) {
        gdt_line("P93", "type of a plain allocation", pa, pa);
        gdt_line("P94", "type of a free page", pa + (long)(2 * PG), pa);
        release_all(pa, 2);
    }
}

// ---- T: every path the shadPS4 changes touch that the sections above do not isolate ----------

static int32_t pdelta_run(Line& l, const char* tag, int32_t r, const PS& a) {
    l.s(tag).c(code(r)).c(':');
    d4(l, a, ps());
    return r;
}

static void run_t() {
    say("== T  touched paths: committed pool block, mixed ranges, nulls, fixed maps, partial ops");
    const uint64_t blk = 0x10000, rsv = 0x200000;
    // T1-T7: the committed block itself, located through VirtualQuery
    {
        uint64_t x = 0;
        sceKernelMemoryPoolExpand(0, g_dmem, blk, blk, &x);
        void* r = 0;
        if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &r) != 0 ||
            sceKernelMemoryPoolCommit(r, blk, 3, 0x3, 0) != 0) {
            say("T1   SKIP reserve/commit");
        } else {
            unsigned char vi[0x60];
            my_memset(vi, 0, sizeof(vi));
            const int32_t rq = sceKernelVirtualQuery(r, 0, vi, 0x48);
            uint64_t off = 0;
            for (int k = 7; k >= 0; k--)
                off = (off << 8) | vi[0x10 + k];
            const bool committed = (vi[0x20] >> 4) & 1, pooled = (vi[0x20] >> 3) & 1;
            const bool located = rq == 0 && committed && off != 0 && off % blk == 0;
            {
                Line l = head("T1", "VirtualQuery of committed VA");
                l.ret("ret", rq).s("pooled=").c(pooled ? '1' : '0').s(" committed=");
                l.c(committed ? '1' : '0').s(" located=").c(located ? '1' : '0').end();
            }
            if (located) {
                const long pa = (long)off;
                q_raw("T2", "exact query of committed block", pa, 0, sizeof(DmemInfo), pa);
                q_raw("T3", "find-next from committed block", pa, 1, sizeof(DmemInfo), pa);
                gdt_line("T4", "type of committed block", pa, pa);
                e_map("T5", "map committed block", pa, blk, 0x3);
                {
                    PS a = ps();
                    Line l = head("T6", "chk / rel committed block");
                    pdelta_run(l, "chk=", sceKernelCheckedReleaseDirectMemory(pa, blk), a);
                    a = ps();
                    pdelta_run(l, " rel=", sceKernelReleaseDirectMemory(pa, blk), a);
                    int32_t pr = 0;
                    l.s(" va=").c(mapped(r, &pr) ? 'M' : '.').end();
                }
            }
            sceKernelMunmap(r, rsv);
        }
    }
    // T8: one release over [allocated A 4 pages][pool block]
    {
        int32_t rr = 0;
        const long c = alloc_al(0, g_dmem, 8, blk, 3, &rr);
        Line l = head("T8", "[A 4pg][pool blk]: chk / rel all");
        if (c < 0) {
            l.s("SKIP alloc").end();
        } else {
            sceKernelReleaseDirectMemory(c, 8 * PG);
            const long a4 = alloc_in(c, c + (long)blk, 4, 3, &rr);
            uint64_t pp = 0;
            const int32_t re =
                sceKernelMemoryPoolExpand((uint64_t)c + blk, (uint64_t)c + 2 * blk, blk, blk, &pp);
            if (a4 != c || re != 0 || pp != (uint64_t)c + blk) {
                l.s("SKIP layout").end();
                release_all(a4, 4);
            } else {
                PS a = ps();
                pdelta_run(l, "chk=", sceKernelCheckedReleaseDirectMemory(c, 2 * blk), a);
                phys_str(l.c(' '), c, 4);
                a = ps();
                pdelta_run(l, " rel=", sceKernelReleaseDirectMemory(c, 2 * blk), a);
                phys_str(l.c(' '), c, 4).end();
                release_all(c, 4);
            }
        }
    }
    // T9: query next to a pool block, [pool blk][A t3][B t3]
    {
        int32_t rr = 0;
        const long c = alloc_al(0, g_dmem, 12, blk, 3, &rr);
        if (c < 0) {
            say("T9   SKIP alloc");
        } else {
            sceKernelReleaseDirectMemory(c, 12 * PG);
            uint64_t pp = 0;
            const int32_t re =
                sceKernelMemoryPoolExpand((uint64_t)c, (uint64_t)c + blk, blk, blk, &pp);
            const long a = alloc_in(c + (long)blk, c + 2 * (long)blk, 4, 3, &rr);
            const long b = alloc_in(c + 2 * (long)blk, c + 3 * (long)blk, 4, 3, &rr);
            if (re != 0 || pp != (uint64_t)c || a != c + (long)blk || b != c + 2 * (long)blk) {
                say("T9   SKIP layout");
            } else {
                q_raw("T9", "[pool][A][B]: query B, rel to B", b, 0, sizeof(DmemInfo), b);
                q_raw("T10", "[pool][A][B]: find-next at pool", c, 1, sizeof(DmemInfo), b);
            }
            release_all(a, 4);
            release_all(b, 4);
        }
    }
    // T11: type of mapped memory
    {
        int32_t rr = 0;
        const long pa = alloc(2, 3, &rr);
        uint8_t* va = pa >= 0 ? map(pa, 2, &rr) : nullptr;
        if (va)
            gdt_line("T11", "type of mapped memory", pa, pa);
        unmap_all(va, 2);
        release_all(pa, 2);
    }
    // T12: available size with one output null; T13: query with a null info pointer
    {
        long ph = -1;
        size_t sz = 0;
        Line l = head("T12", "available: null phys / null size");
        l.ret("np", sceKernelAvailableDirectMemorySize(0, (long)g_dmem, PG, nullptr, &sz));
        l.s("size!=0:").c(sz ? '1' : '0').c(' ');
        l.ret("ns", sceKernelAvailableDirectMemorySize(0, (long)g_dmem, PG, &ph, nullptr));
        l.s("phys set:").c(ph != -1 ? '1' : '0').end();
    }
    {
        int32_t rr = 0;
        const long pa = alloc(1, 3, &rr);
        if (pa >= 0) {
            Line l = head("T13", "query, null info: size 0 / 18");
            l.ret("s0", sceKernelDirectMemoryQuery(pa, 0, nullptr, 0));
            l.ret("s18", sceKernelDirectMemoryQuery(pa, 0, nullptr, 0x18)).end();
            release_all(pa, 1);
        }
    }
    // T14: reserve that cannot be placed (1 TiB): counters must not move
    {
        void* v = 0;
        const PS a = ps();
        Line l = head("T14", "reserve 1T");
        const int32_t r = pdelta_run(l, "", sceKernelMemoryPoolReserve(0, 1ull << 40, 0, 0, &v), a);
        if (r == 0) {
            const PS b = ps();
            pdelta_run(l, " munmap=", sceKernelMunmap(v, 1ull << 40), b);
        }
        l.end();
    }
    // T15: munmap of only the committed 64K inside a reservation
    {
        void* v = 0;
        Line l = head("T15", "munmap committed 64K of a reserve");
        if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) == 0 &&
            sceKernelMemoryPoolCommit(v, blk, 3, 0x3, 0) == 0) {
            const PS a = ps();
            pdelta_run(l, "", sceKernelMunmap(v, blk), a);
            int32_t pr = 0;
            l.s(" va=").c(mapped(v, &pr) ? 'M' : '.');
            const PS b = ps();
            pdelta_run(l, " whole=", sceKernelMunmap(v, rsv), b);
        } else {
            l.s("SKIP");
        }
        l.end();
    }
    // T16: decommit half of a 128K commit, then the rest
    {
        void* v = 0;
        Line l = head("T16", "128K t3: decommit 1st 64K / 2nd");
        if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) == 0 &&
            sceKernelMemoryPoolCommit(v, 2 * blk, 3, 0x3, 0) == 0) {
            PS a = ps();
            pdelta_run(l, "", sceKernelMemoryPoolDecommit(v, blk, 0), a);
            a = ps();
            pdelta_run(l, " ", sceKernelMemoryPoolDecommit((uint8_t*)v + blk, blk, 0), a);
            sceKernelMunmap(v, rsv);
        } else {
            l.s("SKIP");
        }
        l.end();
    }
    // T17: mtypeprotect of committed pool memory to types other than 0 and 3
    {
        static const int types[] = {1, 2, 10};
        Line l = head("T17", "t3 commit, mtype->1,2,10: mt|dc");
        for (int t : types) {
            void* v = 0;
            if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) != 0)
                break;
            if (sceKernelMemoryPoolCommit(v, blk, 3, 0x3, 0) == 0) {
                PS a = ps();
                l.dec(t).c(':');
                pdelta_run(l, "", sceKernelMtypeprotect(v, blk, t, 0x3), a);
                a = ps();
                pdelta_run(l, "|", sceKernelMemoryPoolDecommit(v, blk, 0), a);
                l.c(' ');
            }
            sceKernelMunmap(v, rsv);
        }
        l.end();
    }
    // T18: munmap covering two reservations at once
    {
        void* r1 = 0;
        void* r2 = 0;
        Line l = head("T18", "munmap over two reservations");
        if (sceKernelMemoryPoolReserve(0, rsv, 0, 0, &r1) == 0 &&
            sceKernelMemoryPoolReserve((uint8_t*)r1 + rsv, rsv, 0, 0, &r2) == 0) {
            if (r2 == (uint8_t*)r1 + rsv) {
                const PS a = ps();
                pdelta_run(l, "", sceKernelMunmap(r1, 2 * rsv), a);
            } else {
                l.s("SKIP not adjacent");
                sceKernelMunmap(r1, rsv);
                sceKernelMunmap(r2, rsv);
            }
        } else {
            l.s("SKIP");
        }
        l.end();
    }
    // T19: fixed direct mapping over a reservation that holds a committed block
    {
        void* v = 0;
        int32_t rr = 0;
        const long pa = alloc(4, 3, &rr);
        Line l = head("T19", "MAP_FIXED dmem over reserve | munmap");
        if (pa >= 0 && sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) == 0 &&
            sceKernelMemoryPoolCommit(v, blk, 0, 0x3, 0) == 0) {
            PS a = ps();
            void* at = v;
            pdelta_run(l, "", sceKernelMapDirectMemory(&at, blk, 0x3, 0x10, pa, PG), a);
            l.s(" same=").c(at == v ? '1' : '0');
            a = ps();
            pdelta_run(l, " munmap=", sceKernelMunmap(v, rsv), a);
        } else {
            l.s("SKIP");
        }
        l.end();
        release_all(pa, 4);
    }
    // T20: block stats with sizes other than 16
    {
        static const uint32_t sizes[] = {0, 4, 8, 12, 16, 32};
        Line l = head("T20", "block stats: bytes written per size");
        for (uint32_t sz : sizes) {
            unsigned char buf[0x30];
            my_memset(buf, 0xCC, sizeof(buf));
            const int32_t r = sceKernelMemoryPoolGetBlockStats(buf, sz);
            int last = 0;
            for (int k = 0; k < 0x30; k++)
                if (buf[k] != 0xCC)
                    last = k + 1;
            l.hex(sz).c(':').c(code(r)).hex((uint64_t)last).c(' ');
        }
        l.end();
    }
}

// Window big enough for the length but with no aligned fit inside it; free space right after.
static void run_p54() {
    const uint64_t blk = 0x10000;
    int32_t r = 0;
    const long c = alloc_al(0, g_dmem, 12, blk, 3, &r);
    if (c < 0)
        return setup_failed("P54", "expand window", "alloc", r);
    sceKernelReleaseDirectMemory(c + (long)PG, 11 * PG);
    uint64_t px = 0;
    const int32_t rx =
        sceKernelMemoryPoolExpand((uint64_t)c, (uint64_t)c + 0x14000, blk, blk, &px);
    Line l = head("P54", "expand, window 14000, no fit inside");
    l.ret("ret", rx);
    if (rx == 0)
        l.s("at=").shex((long long)(px - (uint64_t)c));
    l.end();
    release_all(c, 1);
}

// ---- R: positive oversized length on a real allocation ---------------------------------------

static void run_r() {
    say("== R  len 1<<62 from a page above the framebuffer; frees everything above it");
    const uint64_t floor = g_screen.fb_phys >= 0 ? g_screen.fb_phys + g_screen.fb_len : 0;
    int32_t r0 = 0, r1 = 0, r2 = 0;
    const long lo = alloc_in(floor, g_dmem, 1, 3, &r0);
    const long mid = alloc_in(floor, g_dmem, 1, 3, &r1);
    const long hi = alloc_in(floor, g_dmem, 1, 3, &r2);
    if (lo < 0 || mid < 0 || hi < 0) {
        Line l = head("R1", "rel  own page, len 1<<62");
        l.s("SKIP alloc ").ret("a", r0).ret("b", r1).ret("c", r2).end();
    } else {
        say("R1   starting");
        const int32_t r = sceKernelReleaseDirectMemory(mid, 1ull << 62);
        Line l = head("R1", "rel  own page, len 1<<62");
        l.ret("ret", r);
        l.s("first=").c(lo < mid ? '<' : '>').c(allocated(lo) ? 'A' : 'F');
        l.s(" self=").c(allocated(mid) ? 'A' : 'F');
        l.s(" third=").c(hi < mid ? '<' : '>').c(allocated(hi) ? 'A' : 'F').end();
    }
    release_all(lo, 1);
    release_all(mid, 1);
    release_all(hi, 1);
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
        run_d();
        run_q();
        run_v();
        run_e();
        run_f();
        run_h();
        run_h7();
        run_p();
        run_p2();
        run_p4();
        run_p7();
        run_p8();
        run_p9();
        run_t();
        run_p54();
        run_c();
        run_g();
        run_r();
    }
    say("===== DONE =====");
    for (;;)
        sceKernelUsleep(1000000);
    return 0;
}
