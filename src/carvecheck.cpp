// CarveCheck: pass/fail tests for direct memory whose physical range was split behind a mapping by
// another mapping of the same memory (CarvePhysArea), for how the split is reported, and for
// releases of pool memory. Expected results are what a PS4 (FW 12.02) returned in CarveTest.
// Results go to the screen and /data/carvecheck_results.txt (fallback /temp0/).

#include <stddef.h>
#include <stdint.h>

extern "C" {
int32_t sceKernelAllocateDirectMemory(long search_start, long search_end, size_t len,
                                      size_t alignment, int memory_type, long* phys_out);
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, long phys,
                                 size_t alignment);
int32_t sceKernelMapDirectMemory2(void** addr, size_t len, int32_t type, int32_t prot,
                                  int32_t flags, long phys, size_t alignment);
int32_t sceKernelMapNamedDirectMemory(void** addr, size_t len, int32_t prot, int32_t flags,
                                      long phys, size_t alignment, const char* name);
int32_t sceKernelMapFlexibleMemory(void** addr, size_t len, int32_t prot, int32_t flags);
int32_t sceKernelReserveVirtualRange(void** addr, size_t len, int32_t flags, size_t alignment);
int32_t sceKernelMunmap(void* addr, size_t len);
int32_t sceKernelReleaseDirectMemory(long start, size_t len);
// OpenOrbis declares this void; libkernel returns the ioctl result (0x17904: mov eax, ebx).
int32_t sceKernelCheckedReleaseDirectMemory(long start, size_t len);
int32_t sceKernelDirectMemoryQuery(long offset, int32_t flags, void* info, size_t info_size);
int32_t sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int32_t* prot);
int32_t sceKernelEnableDmemAliasing(void);
int32_t sceKernelMtypeprotect(const void* addr, size_t len, int32_t mtype, int32_t prot);
int32_t sceKernelMprotect(const void* addr, size_t len, int32_t prot);
int32_t sceKernelGetDirectMemoryType(long start, int32_t* type, long* region_start,
                                     long* region_end);
int32_t sceKernelVirtualQuery(const void* addr, int32_t flags, void* info, size_t info_size);
// Not in the OpenOrbis headers; argument registers from libkernel 0x19d80 / 0x19f30.
int32_t sceKernelMemoryPoolExpand(uint64_t search_start, uint64_t search_end, uint64_t len,
                                  uint64_t alignment, uint64_t* phys_out);
int32_t sceKernelMemoryPoolReserve(void* addr_in, uint64_t len, uint64_t alignment, int32_t flags,
                                   void** addr_out);
int32_t sceKernelMemoryPoolCommit(void* addr, uint64_t len, int32_t type, int32_t prot,
                                  int32_t flags);
int32_t sceKernelMemoryPoolDecommit(void* addr, uint64_t len, int32_t flags);
long sceKernelGetDirectMemorySize(void);
int sceKernelUsleep(unsigned int usec);
int sceKernelOpen(const char* path, int flags, unsigned short mode);
long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
long sceKernelRead(int fd, void* buf, unsigned long nbytes);
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
void* memset(void* d, int v, size_t n);
}

// Referencing memset links libc.a's static copy, which crt's startup then uses too.
static void* my_memset(void* d, int v, unsigned long n) {
    return memset(d, v, n);
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
            if (has(s, "FAIL"))
                col = 0x80FF5050u;
            else if (has(s, "PASS"))
                col = 0x8040E060u;
            else if (s[0] == '=')
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

static const char* g_log_path = "/data/carvecheck_results.txt";
static const char* g_state_path = "/data/carvecheck.state";

static void log_pick_dir() {
    int fd = sceKernelOpen(g_log_path, 0x0209, 0777);
    if (fd < 0) {
        g_log_path = "/temp0/carvecheck_results.txt";
        g_state_path = "/temp0/carvecheck.state";
        fd = sceKernelOpen(g_log_path, 0x0209, 0777);
    }
    if (fd >= 0)
        sceKernelClose(fd);
}

static void log_truncate() {
    const int fd = sceKernelOpen(g_log_path, 0x0601, 0777);
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

// One result line. A line that does not fit ends in "[TRUNCATED]" rather than losing its tail
// silently.
struct Line {
    static constexpr int kCap = 4000;
    char b[kCap + 32];
    int p = 0;
    bool cut = false;
    Line& s(const char* t) {
        while (*t)
            c(*t++);
        return *this;
    }
    Line& c(char ch) {
        if (p < kCap)
            b[p++] = ch;
        else
            cut = true;
        return *this;
    }
    Line& to(int col) {
        while (p < col && p < kCap)
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
        if (cut) {
            for (const char* t = " [TRUNCATED]"; *t; t++)
                b[p++] = *t;
        }
        b[p++] = '\n';
        emit(b, p);
    }
};


static void say(const char* t) {
    Line l;
    l.s(t).end();
}

// ---- memory helpers ---------------------------------------------------------------------------

static const uint64_t PG = 0x4000;
static const int NP = 8; // pages per test block
static const uint64_t POOL_BLK = 0x10000, POOL_RSV = 0x200000;
static uint64_t g_dmem = 0;

struct DmemInfo { // OrbisQueryInfo
    uint64_t start, end;
    int32_t mtype, pad;
};

static bool allocated(long pa) {
    DmemInfo q;
    my_memset(&q, 0, sizeof(q));
    if (sceKernelDirectMemoryQuery(pa, 0, &q, sizeof(q)) != 0)
        return false;
    return q.start <= (uint64_t)pa && (uint64_t)pa < q.end;
}

static bool mapped(const void* va, int32_t* prot) {
    void* s = 0;
    void* e = 0;
    int32_t p = 0;
    if (sceKernelQueryMemoryProtection((void*)va, &s, &e, &p) != 0)
        return false;
    if (prot)
        *prot = p;
    return true;
}

static void stamp(uint8_t* va, int pages, uint32_t tag) {
    for (int i = 0; i < pages; i++)
        *(volatile uint32_t*)(va + i * PG) = tag + (uint32_t)i;
}

static void unmap_pages(uint8_t* va, int pages) {
    if (!va)
        return;
    for (int i = 0; i < pages; i++)
        if (mapped(va + i * PG, nullptr))
            sceKernelMunmap(va + i * PG, PG);
}

static void release_pages(long pa, int pages) {
    if (pa < 0)
        return;
    for (int i = 0; i < pages; i++)
        if (allocated(pa + (long)i * (long)PG))
            sceKernelReleaseDirectMemory(pa + (long)i * (long)PG, PG);
}

// pages test pages with a released page on both sides, so nothing else is adjacent
static long block_new(int pages) {
    long base = -1;
    if (sceKernelAllocateDirectMemory(0, (long)g_dmem, (pages + 2) * PG, PG, 3, &base) != 0)
        return -1;
    sceKernelReleaseDirectMemory(base, PG);
    sceKernelReleaseDirectMemory(base + (long)(pages + 1) * (long)PG, PG);
    return base + (long)PG;
}

static uint8_t* map_at(void* fixed, long pa, int pages, int32_t prot, int32_t* r) {
    void* va = fixed;
    *r = sceKernelMapDirectMemory(&va, pages * PG, prot, fixed ? 0x10 : 0, pa, PG);
    return *r == 0 ? (uint8_t*)va : nullptr;
}

// ---- checks -------------------------------------------------------------------------------------
// A test calls these; each failed check adds its reason to g_why.

static bool g_ok;
static Line* g_why;

static void fail(const char* what) {
    g_ok = false;
    g_why->s(what).s("; ");
}

static void want_ret(const char* what, int32_t got, int32_t want) {
    if (got == want)
        return;
    g_ok = false;
    g_why->s(what).s(" returned ").hex((uint32_t)got).s(", PS4 ").hex((uint32_t)want).s("; ");
}

static void want_unmapped(const char* what, uint8_t* va, int first, int pages) {
    for (int i = first; i < first + pages; i++)
        if (mapped(va + i * PG, nullptr)) {
            g_ok = false;
            g_why->s(what).s(" page ").dec(i).s(" still mapped; ");
            return;
        }
}

// mapped, readable, and holding what stamp() wrote
static void want_intact(const char* what, uint8_t* va, int first, int pages, uint32_t tag) {
    for (int i = first; i < first + pages; i++) {
        int32_t prot = 0;
        if (!mapped(va + i * PG, &prot) || !(prot & 1)) {
            g_ok = false;
            g_why->s(what).s(" page ").dec(i).s(" not mapped; ");
            return;
        }
        if (*(volatile uint32_t*)(va + i * PG) != tag + (uint32_t)i) {
            g_ok = false;
            g_why->s(what).s(" page ").dec(i).s(" data changed; ");
            return;
        }
    }
}

static void want_phys(const char* what, long pa, int pages, bool alloc) {
    for (int i = 0; i < pages; i++)
        if (allocated(pa + (long)i * (long)PG) != alloc) {
            g_ok = false;
            g_why->s(what).s(" page ").dec(i).s(alloc ? " not allocated; " : " still allocated; ");
            return;
        }
}

// GetDirectMemoryType and DirectMemoryQuery for every page from first: the region [s, e) of type t
static void want_region(long pa, int first, int pages, long s, long e, int32_t t) {
    for (int i = first; i < first + pages; i++) {
        const long at = pa + (long)i * (long)PG;
        int32_t gt = -1;
        long gs = 0, ge = 0;
        const int32_t rg = sceKernelGetDirectMemoryType(at, &gt, &gs, &ge);
        DmemInfo q;
        my_memset(&q, 0, sizeof(q));
        const int32_t rq = sceKernelDirectMemoryQuery(at, 0, &q, sizeof(q));
        if (rg != 0 || gs != s || ge != e || gt != t) {
            g_ok = false;
            g_why->s("GetDirectMemoryType page ").dec(i).s(" gave ");
            if (rg != 0)
                g_why->s("error ").hex((uint32_t)rg);
            else
                g_why->dec((gs - pa) / (long)PG)
                    .c('-')
                    .dec((ge - pa) / (long)PG)
                    .s(" type ")
                    .dec(gt);
            g_why->s(", PS4 ").dec((s - pa) / (long)PG).c('-').dec((e - pa) / (long)PG);
            g_why->s(" type ").dec(t).s("; ");
            return;
        }
        if (rq != 0 || (long)q.start != s || (long)q.end != e || q.mtype != t) {
            g_ok = false;
            g_why->s("DirectMemoryQuery page ").dec(i).s(" gave ");
            if (rq != 0)
                g_why->s("error ").hex((uint32_t)rq);
            else
                g_why->dec(((long)q.start - pa) / (long)PG)
                    .c('-')
                    .dec(((long)q.end - pa) / (long)PG)
                    .s(" type ")
                    .dec(q.mtype);
            g_why->s(", PS4 ").dec((s - pa) / (long)PG).c('-').dec((e - pa) / (long)PG);
            g_why->s(" type ").dec(t).s("; ");
            return;
        }
    }
}

// ---- fixtures -----------------------------------------------------------------------------------

// A block of NP pages mapped once (A) and stamped
struct Fix {
    long pa = -1;
    uint8_t* a = nullptr;
    uint32_t tag = 0;
};

static bool fix_new(Fix& f, uint32_t tag, int32_t prot = 0x3, void* fixed = nullptr) {
    f.tag = tag;
    f.pa = block_new(NP);
    if (f.pa < 0) {
        fail("setup: allocation failed");
        return false;
    }
    int32_t r = 0;
    f.a = map_at(fixed, f.pa, NP, prot, &r);
    if (!f.a) {
        want_ret("setup: map", r, 0);
        release_pages(f.pa, NP);
        f.pa = -1;
        return false;
    }
    stamp(f.a, NP, tag);
    return true;
}

static void fix_drop(Fix& f) {
    unmap_pages(f.a, NP);
    release_pages(f.pa, NP);
}

// a second mapping of pages first..first+pages-1, unmapped again
static bool split_by_alias(Fix& f, int first, int pages) {
    int32_t r = 0;
    uint8_t* b = map_at(nullptr, f.pa + first * (long)PG, pages, 0x3, &r);
    if (!b) {
        want_ret("setup: alias map", r, 0);
        return false;
    }
    want_ret("setup: alias munmap", sceKernelMunmap(b, pages * PG), 0);
    return g_ok;
}

struct Pool {
    uint64_t pp = 0;
    uint8_t* va = nullptr;
};

static bool pool_commit(Pool& p) {
    const int32_t re = sceKernelMemoryPoolExpand(0, g_dmem, 2 * POOL_BLK, POOL_BLK, &p.pp);
    want_ret("setup: pool expand", re, 0);
    if (re != 0)
        return false;
    void* v = nullptr;
    const int32_t rr = sceKernelMemoryPoolReserve(0, POOL_RSV, 0, 0, &v);
    want_ret("setup: pool reserve", rr, 0);
    if (rr != 0)
        return false;
    p.va = (uint8_t*)v;
    want_ret("setup: pool commit", sceKernelMemoryPoolCommit(p.va, POOL_BLK, 3, 0x3, 0), 0);
    return g_ok;
}

static void pool_drop(Pool& p) {
    if (p.va) {
        sceKernelMemoryPoolDecommit(p.va, POOL_BLK, 0);
        sceKernelMunmap(p.va, POOL_RSV);
    }
    if (p.pp)
        sceKernelReleaseDirectMemory((long)p.pp, 2 * POOL_BLK);
}

// ---- tests: a range split behind the mapping ----------------------------------------------------

static void t_munmap_after_alias(uint32_t tag, int first, int pages) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, first, pages)) {
        want_ret("munmap A", sceKernelMunmap(f.a, NP * PG), 0);
        want_unmapped("A", f.a, 0, NP);
        want_phys("memory", f.pa, NP, true);
    }
    fix_drop(f);
}

static void a01(uint32_t tag) {
    t_munmap_after_alias(tag, 3, 2);
}
static void a04(uint32_t tag) {
    t_munmap_after_alias(tag, 0, 2);
}
static void a05(uint32_t tag) {
    t_munmap_after_alias(tag, 6, 2);
}

static void t_munmap_with_map2_alias(uint32_t tag, int32_t type) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    void* v = nullptr;
    const int32_t r = sceKernelMapDirectMemory2(&v, 2 * PG, type, 0x3, 0, f.pa + 3 * (long)PG, PG);
    want_ret("MapDirectMemory2 alias", r, 0);
    uint8_t* b = r == 0 ? (uint8_t*)v : nullptr;
    if (b) {
        want_ret("munmap A", sceKernelMunmap(f.a, NP * PG), 0);
        want_unmapped("A", f.a, 0, NP);
        want_intact("alias", b, 0, 2, tag + 3);
        unmap_pages(b, 2);
    }
    fix_drop(f);
}

static void a02(uint32_t tag) {
    t_munmap_with_map2_alias(tag, 3);
}
static void a13(uint32_t tag) {
    t_munmap_with_map2_alias(tag, 0);
}

static void a03(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    void* v = nullptr;
    const int32_t r =
        sceKernelMapNamedDirectMemory(&v, 2 * PG, 0x3, 0, f.pa + 3 * (long)PG, PG, "carvecheck");
    want_ret("named alias", r, 0);
    if (r == 0) {
        want_ret("alias munmap", sceKernelMunmap(v, 2 * PG), 0);
        want_ret("munmap A", sceKernelMunmap(f.a, NP * PG), 0);
        want_unmapped("A", f.a, 0, NP);
    }
    fix_drop(f);
}

static void a06(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    uint8_t* b[3] = {};
    for (int k = 0; k < 3; k++) {
        int32_t r = 0;
        b[k] = map_at(nullptr, f.pa + (5 + k) * (long)PG, 1, 0x3, &r);
        want_ret("alias map", r, 0);
    }
    for (int k = 0; k < 3; k++)
        if (b[k])
            want_ret("alias munmap", sceKernelMunmap(b[k], PG), 0);
    if (g_ok) {
        want_ret("munmap A", sceKernelMunmap(f.a, NP * PG), 0);
        want_unmapped("A", f.a, 0, NP);
    }
    fix_drop(f);
}

static void a07(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, 3, 2)) {
        want_ret("mtypeprotect A", sceKernelMtypeprotect(f.a, NP * PG, 3, 0x3), 0);
        want_intact("A", f.a, 0, NP, tag);
    }
    fix_drop(f);
}

static void a08(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, 3, 2)) {
        want_ret("munmap A pages 2-4", sceKernelMunmap(f.a + 2 * PG, 3 * PG), 0);
        want_unmapped("A", f.a, 2, 3);
        want_intact("A", f.a, 0, 2, tag);
        want_intact("A", f.a, 5, 3, tag);
    }
    fix_drop(f);
}

static void t_release_after_alias(uint32_t tag, bool checked) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, 3, 2)) {
        want_ret(checked ? "checked release" : "release",
                 checked ? sceKernelCheckedReleaseDirectMemory(f.pa, NP * PG)
                         : sceKernelReleaseDirectMemory(f.pa, NP * PG),
                 0);
        want_phys("memory", f.pa, NP, false);
        want_unmapped("A", f.a, 0, NP);
    }
    fix_drop(f);
}

static void a09(uint32_t tag) {
    t_release_after_alias(tag, false);
}
static void a10(uint32_t tag) {
    t_release_after_alias(tag, true);
}

static void a11(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, 3, 2)) {
        want_ret("mtypeprotect A pages 2-4", sceKernelMtypeprotect(f.a + 2 * PG, 3 * PG, 3, 0x3),
                 0);
        want_intact("A", f.a, 0, NP, tag);
    }
    fix_drop(f);
}

// A and a second mapping C side by side in one reserved range, one mtypeprotect over both
static void a12(uint32_t tag) {
    const long cpa = block_new(4);
    if (cpa < 0) {
        fail("setup: allocation failed");
        return;
    }
    void* rsv = nullptr;
    const int32_t rr = sceKernelReserveVirtualRange(&rsv, (NP + 4) * PG, 0, PG);
    want_ret("setup: reserve", rr, 0);
    Fix f;
    if (rr == 0 && fix_new(f, tag, 0x33, rsv)) {
        int32_t r = 0;
        uint8_t* c = map_at((uint8_t*)rsv + NP * PG, cpa, 4, 0x33, &r);
        want_ret("setup: map C", r, 0);
        if (c) {
            stamp(c, 4, tag + 0x80);
            if (split_by_alias(f, 3, 2)) {
                want_ret("mtypeprotect A+C", sceKernelMtypeprotect(rsv, (NP + 4) * PG, 3, 0x33), 0);
                want_intact("A", f.a, 0, NP, tag);
                want_intact("C", c, 0, 4, tag + 0x80);
            }
            unmap_pages(c, 4);
        }
        fix_drop(f);
    }
    if (rsv)
        sceKernelMunmap(rsv, (NP + 4) * PG);
    release_pages(cpa, 4);
}

static void t_pool_release_in_commit(bool mtype) {
    Pool p;
    if (pool_commit(p)) {
        want_ret("release in the commit", sceKernelReleaseDirectMemory((long)p.pp + (long)PG, PG),
                 0);
        if (mtype)
            want_ret("mtypeprotect", sceKernelMtypeprotect(p.va, POOL_BLK, 3, 0x3), 0);
        want_ret("decommit", sceKernelMemoryPoolDecommit(p.va, POOL_BLK, 0), 0);
    }
    pool_drop(p);
}

static void a14(uint32_t) {
    t_pool_release_in_commit(false);
}
static void a15(uint32_t) {
    t_pool_release_in_commit(true);
}

// ---- tests: how the range is reported -----------------------------------------------------------

static void r01(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    if (split_by_alias(f, 3, 2))
        want_region(f.pa, 0, NP, f.pa, f.pa + NP * (long)PG, 3);
    fix_drop(f);
}

static void r02(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    want_ret("munmap A pages 2-4", sceKernelMunmap(f.a + 2 * PG, 3 * PG), 0);
    want_region(f.pa, 0, NP, f.pa, f.pa + NP * (long)PG, 3);
    fix_drop(f);
}

static void r03(uint32_t tag) {
    Fix f;
    if (!fix_new(f, tag))
        return;
    want_ret("mtypeprotect A pages 2-4", sceKernelMtypeprotect(f.a + 2 * PG, 3 * PG, 3, 0x3), 0);
    want_region(f.pa, 0, NP, f.pa, f.pa + NP * (long)PG, 3);
    fix_drop(f);
}

// pages 4-7 released and allocated again on their own, then the block mapped as one
static void t_two_allocations(int32_t type2) {
    const long pa = block_new(NP);
    if (pa < 0) {
        fail("setup: allocation failed");
        return;
    }
    const long half = pa + 4 * (long)PG;
    long q = -1;
    want_ret("setup: release pages 4-7", sceKernelReleaseDirectMemory(half, 4 * PG), 0);
    want_ret("setup: allocate pages 4-7",
             sceKernelAllocateDirectMemory(half, half + 4 * (long)PG, 4 * PG, PG, type2, &q), 0);
    if (g_ok && q != half)
        fail("setup: pages 4-7 allocated elsewhere");
    int32_t r = 0;
    uint8_t* a = g_ok ? map_at(nullptr, pa, NP, 0x3, &r) : nullptr;
    if (g_ok)
        want_ret("setup: map", r, 0);
    if (a) {
        if (type2 == 3) {
            want_region(pa, 0, NP, pa, pa + NP * (long)PG, 3);
            want_ret("munmap A pages 4-7", sceKernelMunmap(a + 4 * PG, 4 * PG), 0);
            want_region(pa, 0, NP, pa, pa + NP * (long)PG, 3);
        } else {
            want_region(pa, 0, 4, pa, half, 3);
            want_region(pa, 4, 4, half, pa + NP * (long)PG, type2);
        }
    }
    unmap_pages(a, NP);
    release_pages(pa, NP);
}

static void r04(uint32_t) {
    t_two_allocations(3);
}
static void r05(uint32_t) {
    t_two_allocations(0);
}

// ---- tests: releases --------------------------------------------------------------------------

static void l01(uint32_t) {
    uint64_t pp = 0, pp2 = 0;
    const int32_t re = sceKernelMemoryPoolExpand(0, g_dmem, 2 * POOL_BLK, POOL_BLK, &pp);
    want_ret("setup: pool expand", re, 0);
    if (re != 0)
        return;
    want_ret("release", sceKernelReleaseDirectMemory((long)pp, 2 * POOL_BLK), 0);
    const int32_t re2 = sceKernelMemoryPoolExpand(0, g_dmem, 2 * POOL_BLK, POOL_BLK, &pp2);
    want_ret("expand again", re2, 0);
    if (re2 == 0 && pp2 == pp)
        fail("the next expand got the released memory back");
    sceKernelReleaseDirectMemory((long)pp, 2 * POOL_BLK);
    if (re2 == 0)
        sceKernelReleaseDirectMemory((long)pp2, 2 * POOL_BLK);
}

static void l02(uint32_t) {
    uint64_t pp = 0;
    const int32_t re = sceKernelMemoryPoolExpand(0, g_dmem, 2 * POOL_BLK, POOL_BLK, &pp);
    want_ret("setup: pool expand", re, 0);
    if (re != 0)
        return;
    want_ret("checked release", sceKernelCheckedReleaseDirectMemory((long)pp, 2 * POOL_BLK),
             (int32_t)0x80020002);
    sceKernelReleaseDirectMemory((long)pp, 2 * POOL_BLK);
}

static void l03(uint32_t) {
    Pool p;
    if (pool_commit(p)) {
        want_ret("decommit", sceKernelMemoryPoolDecommit(p.va, POOL_BLK, 0), 0);
        want_ret("munmap", sceKernelMunmap(p.va, POOL_RSV), 0);
        p.va = nullptr;
        want_ret("release", sceKernelReleaseDirectMemory((long)p.pp, 2 * POOL_BLK), 0);
    }
    pool_drop(p);
}

static void l04(uint32_t) {
    const long pa = block_new(2);
    if (pa < 0) {
        fail("setup: allocation failed");
        return;
    }
    want_ret("setup: release page 0", sceKernelCheckedReleaseDirectMemory(pa, PG), 0);
    want_ret("release page 0 again", sceKernelReleaseDirectMemory(pa, PG), 0);
    if (!allocated(pa + (long)PG))
        fail("page 1 released too");
    release_pages(pa, 2);
}

// ---- runner -------------------------------------------------------------------------------------

struct Test {
    const char* id;
    char group;
    const char* name;
    void (*fn)(uint32_t);
};

static const Test TESTS[] = {
    {"A01", 'A', "munmap whole mapping; split by alias of pages 3-4, unmapped", a01},
    {"A02", 'A', "munmap whole mapping; MapDirectMemory2 alias of pages 3-4 kept", a02},
    {"A03", 'A', "munmap whole mapping; named alias of pages 3-4, unmapped", a03},
    {"A04", 'A', "munmap whole mapping; split at pages 0-1", a04},
    {"A05", 'A', "munmap whole mapping; split at pages 6-7", a05},
    {"A06", 'A', "munmap whole mapping; aliases of pages 5, 6, 7 unmapped (RDR2)", a06},
    {"A07", 'A', "mtypeprotect whole mapping; split by alias of pages 3-4", a07},
    {"A08", 'A', "munmap pages 2-4, across the split", a08},
    {"A09", 'A', "ReleaseDirectMemory whole block; split by alias of pages 3-4", a09},
    {"A10", 'A', "CheckedReleaseDirectMemory whole block; split by alias", a10},
    {"A11", 'A', "mtypeprotect pages 2-4, across the split", a11},
    {"A12", 'A', "mtypeprotect over the mapping and the next one; first split (RDR2)", a12},
    {"A13", 'A', "munmap whole mapping; MapDirectMemory2 alias of pages 3-4, type 0", a13},
    {"A14", 'A', "PoolDecommit; a page of the commit released first", a14},
    {"A15", 'A', "mtypeprotect a pool commit; a page of it released first", a15},
    {"R01", 'R', "after an alias of pages 3-4 was mapped and unmapped", r01},
    {"R02", 'R', "after munmap of pages 2-4", r02},
    {"R03", 'R', "after mtypeprotect of pages 2-4 to the same type", r03},
    {"R04", 'R', "two adjacent allocations of type 3, then munmap of pages 4-7", r04},
    {"R05", 'R', "two adjacent allocations of types 3 and 0: one region each", r05},
    {"L01", 'L', "ReleaseDirectMemory of pool memory: 0, and it stays in the pool", l01},
    {"L02", 'L', "CheckedReleaseDirectMemory of pool memory: ENOENT", l02},
    {"L03", 'L', "ReleaseDirectMemory of pool memory after decommit and munmap", l03},
    {"L04", 'L', "ReleaseDirectMemory starting on a free page stays inside its range", l04},
};
static const int NTESTS = (int)(sizeof(TESTS) / sizeof(TESTS[0]));

static const char* group_name(char g) {
    return g == 'A' ? "split range" : g == 'R' ? "region reporting" : "releases";
}

static const char* group_goal(char g) {
    return g == 'A' ? "calls on a mapping whose memory another mapping split cover the whole range"
           : g == 'R' ? "GetDirectMemoryType and DirectMemoryQuery report the whole allocation"
                      : "releases leave pool memory, and memory past their range, alone";
}

// carvecheck.state next to the results: the build, the next test, the test running when the app
// ended, tests that ended one launch (run once more), tests that ended two (failed), and every
// result so far. A new CK_BUILD starts over.
#define CK_BUILD "check-1"

struct CkState {
    char build[16];
    int32_t next, inflight;
    uint32_t once, ended, passed, done;
};

static void state_save(const CkState* st) {
    const int fd = sceKernelOpen(g_state_path, 0x601, 0777);
    if (fd < 0)
        return;
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}

static bool state_load(CkState* st) {
    const int fd = sceKernelOpen(g_state_path, 0, 0);
    if (fd < 0)
        return false;
    const long n = sceKernelRead(fd, st, sizeof(*st));
    sceKernelClose(fd);
    if (n != (long)sizeof(*st))
        return false;
    for (int i = 0; CK_BUILD[i] || st->build[i]; i++)
        if (i >= 15 || CK_BUILD[i] != st->build[i])
            return false;
    return true;
}

static void report(int i, bool ok, const Line* why) {
    Line l;
    l.s(ok ? "PASS  " : "FAIL  ").s(TESTS[i].id).s("  ").s(TESTS[i].name);
    if (!ok && why && why->p > 0) {
        l.s("  -- ");
        for (int k = 0; k < why->p; k++)
            l.c(why->b[k]);
    }
    l.end();
}

// After a relaunch the screen starts empty: put the test results of the earlier launches back on
// it. Screen only; the results file already holds all of it.
static char g_prev[0x10000];
static long g_prev_len = 0;

static void load_results() {
    g_prev_len = 0;
    const int fd = sceKernelOpen(g_log_path, 0, 0);
    if (fd < 0)
        return;
    long n;
    while (g_prev_len < (long)sizeof(g_prev) &&
           (n = sceKernelRead(fd, g_prev + g_prev_len,
                              sizeof(g_prev) - (unsigned long)g_prev_len)) > 0)
        g_prev_len += n;
    sceKernelClose(fd);
}

static bool is_test_result(const char* l, long n) {
    const bool verdict = n > 8 && (l[0] == 'P' || l[0] == 'F') && l[4] == ' ' && l[5] == ' ';
    return verdict && (l[6] == 'A' || l[6] == 'R' || l[6] == 'L') && l[7] >= '0' && l[7] <= '9';
}

static void screen_line(const char* t) {
    Line l;
    l.s(t).c('\n');
    g_screen.push(l.b, l.p);
}

static void show_previous_results() {
    load_results();
    const long total = g_prev_len;
    if (!g_screen.ok)
        return;
    screen_line("===== EARLIER RESULTS =====");
    long start = 0;
    for (long i = 0; i < total; i++) {
        if (g_prev[i] != '\n')
            continue;
        if (is_test_result(g_prev + start, i - start))
            g_screen.push(g_prev + start, (int)(i - start + 1));
        start = i + 1;
    }
    g_screen.render();
}

// The reasons a test failed, from its last FAIL line in the results file; empty if there is none.
static void fail_reasons(const char* id, const char** at, long* len) {
    *at = nullptr;
    *len = 0;
    long start = 0;
    for (long i = 0; i < g_prev_len; i++) {
        if (g_prev[i] != '\n')
            continue;
        const char* l = g_prev + start;
        const long n = i - start;
        start = i + 1;
        if (n < 10 || l[0] != 'F' || l[6] != id[0] || l[7] != id[1] || l[8] != id[2])
            continue;
        for (long k = 9; k + 4 <= n; k++)
            if (l[k] == ' ' && l[k + 1] == '-' && l[k + 2] == '-' && l[k + 3] == ' ') {
                *at = l + k + 4;
                *len = n - (k + 4);
                break;
            }
    }
}

// Every test under its group, failures with each failed check on its own line.
static void summary(const CkState& st) {
    load_results();
    say("===== RESULT =====");
    int all = 0, all_ok = 0;
    for (const char* gp = "ARL"; *gp; gp++) {
        const char g = *gp;
        int n = 0, ok = 0;
        for (int i = 0; i < NTESTS; i++)
            if (TESTS[i].group == g && ((st.done >> i) & 1)) {
                n++;
                ok += (st.passed >> i) & 1;
            }
        all += n;
        all_ok += ok;
        Line h;
        h.s(ok == n ? "PASS  " : "FAIL  ").s(group_name(g)).s(" ").dec(ok).c('/').dec(n);
        h.s(": ").s(group_goal(g)).end();
        for (int i = 0; i < NTESTS; i++) {
            if (TESTS[i].group != g || !((st.done >> i) & 1))
                continue;
            const bool pass = (st.passed >> i) & 1;
            Line l;
            l.s(pass ? "  PASS  " : "  FAIL  ").s(TESTS[i].id).s("  ").s(TESTS[i].name).end();
            if (pass)
                continue;
            const char* why;
            long len;
            fail_reasons(TESTS[i].id, &why, &len);
            long k = 0;
            while (k < len) {
                Line r;
                r.s("          ");
                while (k < len && !(why[k] == ';' && (k + 1 >= len || why[k + 1] == ' ')))
                    r.c(why[k++]);
                k += 2;
                r.end();
            }
        }
    }
    Line l;
    l.s(all_ok == all ? "PASS  " : "FAIL  ").s("total ").dec(all_ok).c('/').dec(all).end();
}

// Last line on screen: a relaunch only resumes, so a fresh run needs the state file gone.
static void show_fresh_run_note() {
    if (!g_screen.ok)
        return;
    Line l;
    l.s("== for a fresh run delete ").s(g_state_path).c('\n');
    g_screen.push(l.b, l.p);
    g_screen.render();
}

int main(void) {
    g_screen.init();
    log_pick_dir();
    g_dmem = (uint64_t)sceKernelGetDirectMemorySize();

    CkState st;
    my_memset(&st, 0, sizeof(st));
    const bool resumed = state_load(&st);
    if (!resumed) {
        my_memset(&st, 0, sizeof(st));
        for (int i = 0; CK_BUILD[i] && i < 15; i++)
            st.build[i] = CK_BUILD[i];
        st.inflight = -1;
        log_truncate();
        state_save(&st);
    }

    if (resumed)
        show_previous_results();
    say("===== CARVE CHECK =====");
    {
        Line l;
        l.s("build=").s(CK_BUILD).s(resumed ? " launch=resumed" : " launch=fresh");
        l.s(" next=").s(st.next < NTESTS ? TESTS[st.next].id : "done").end();
    }

    if (st.inflight >= 0 && st.inflight < NTESTS) {
        const int i = st.inflight;
        if ((st.once >> i) & 1) {
            st.ended |= 1u << i;
            st.done |= 1u << i;
            Line why;
            why.s("ended the app on two launches");
            report(i, false, &why);
            st.next = i + 1;
        } else {
            st.once |= 1u << i;
            Line l;
            l.s("RETRY ").s(TESTS[i].id).s("  ended the last launch, running it once more").end();
            st.next = i;
        }
        st.inflight = -1;
        state_save(&st);
    }

    const bool nothing_to_run = st.next >= NTESTS;
    if (!nothing_to_run) {
        // Every test here maps the same memory twice; SDK 2.50 and later require this first.
        sceKernelEnableDmemAliasing();
        char group = 0;
        for (int i = st.next; i < NTESTS; i++) {
            if (TESTS[i].group != group) {
                group = TESTS[i].group;
                Line l;
                l.s("== ").s(group_name(group)).s(": ").s(group_goal(group)).end();
            }
            st.inflight = i;
            state_save(&st);
            Line why;
            g_ok = true;
            g_why = &why;
            TESTS[i].fn(0x43430000u | ((uint32_t)i << 8));
            report(i, g_ok, &why);
            st.once &= ~(1u << i);
            st.done |= 1u << i;
            if (g_ok)
                st.passed |= 1u << i;
            st.inflight = -1;
            st.next = i + 1;
            state_save(&st);
        }
    }
    summary(st);
    show_fresh_run_note();
    for (;;)
        sceKernelUsleep(1000000);
    return 0;
}
