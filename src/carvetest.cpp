// Direct memory physical-area test: a mapping whose physical range is split by another mapping of
// the same memory, then mtypeprotect, munmap, release and pool decommit on the first mapping.
// Results go to the screen and /data/carve_results.txt (fallback /temp0/). Addresses are page
// offsets from each probe's own block, so a PS4 log and a shadPS4 log diff line for line.
//   phys:  A allocated  F free
//   va:    M mapped, data intact   m mapped, data changed   n mapped, no CPU read
//          r mapped, backing released (not read)   . unmapped
//   o:     = VirtualQuery offset is the expected physical page   # it is not   - not reported
//   gt/dq: runs of pages that GetDirectMemoryType / DirectMemoryQuery report alike,
//          start-end:type as page offsets from the block, or page!ret for a failing page

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

static const char* g_log_path = "/data/carve_results.txt";
static const char* g_state_path = "/data/carve.state";

static void log_pick_dir() {
    int fd = sceKernelOpen(g_log_path, 0x0209, 0777);
    if (fd < 0) {
        g_log_path = "/temp0/carve_results.txt";
        g_state_path = "/temp0/carve.state";
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

static Line head(const char* id, const char* name) {
    Line l;
    l.s(id).to(5).s(name).c(' ').to(40);
    return l;
}

// ---- memory helpers ---------------------------------------------------------------------------

static const uint64_t PG = 0x4000;
static const int NP = 8; // pages per test block
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

static uint64_t le64(const unsigned char* b) {
    uint64_t v = 0;
    for (int k = 7; k >= 0; k--)
        v = (v << 8) | b[k];
    return v;
}

// VirtualQuery of one page: physical address it reports for va (-1 if none), memory type (-1)
static void vq_page(const void* va, long* phys, int32_t* type) {
    unsigned char vi[0x60];
    my_memset(vi, 0, sizeof(vi));
    *phys = -1;
    *type = -1;
    if (sceKernelVirtualQuery(va, 0, vi, 0x48) != 0)
        return;
    *type = (int32_t)((uint32_t)vi[0x1C] | ((uint32_t)vi[0x1D] << 8) | ((uint32_t)vi[0x1E] << 16) |
                      ((uint32_t)vi[0x1F] << 24));
    if (vi[0x20] & 0x0A) // direct or pooled
        *phys = (long)(le64(vi + 0x10) + ((uint64_t)va - le64(vi)));
}

static void stamp(uint8_t* va, int pages, uint32_t tag) {
    for (int i = 0; i < pages; i++)
        *(volatile uint32_t*)(va + i * PG) = tag + (uint32_t)i;
}

static char hexc(uint32_t v) {
    v &= 0xF;
    return (char)(v < 10 ? '0' + v : 'A' + v - 10);
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

// ---- readback strings -------------------------------------------------------------------------

static Line& rel_page(Line& l, long addr, long base) {
    const long d = addr - base;
    if (d < 0)
        l.c('-');
    const long a = d < 0 ? -d : d;
    l.dec(a / (long)PG);
    if (a % (long)PG)
        l.c('+').hex((uint64_t)(a % (long)PG));
    return l;
}

// One token per run of pages that the query reports alike.
static Line& region_str(Line& l, const char* tag, long pa, int pages, bool gt) {
    l.s(tag).c('[');
    bool first = true;
    int32_t pr = 0, pt = 0;
    long ps = 0, pe = 0;
    for (int i = 0; i < pages; i++) {
        const long at = pa + (long)i * (long)PG;
        int32_t r, t = 0;
        long s = 0, e = 0;
        if (gt) {
            r = sceKernelGetDirectMemoryType(at, &t, &s, &e);
        } else {
            DmemInfo q;
            my_memset(&q, 0, sizeof(q));
            r = sceKernelDirectMemoryQuery(at, 0, &q, sizeof(q));
            s = (long)q.start;
            e = (long)q.end;
            t = q.mtype;
        }
        if (r != 0) {
            s = e = 0;
            t = 0;
        }
        if (i > 0 && r == pr && s == ps && e == pe && t == pt)
            continue;
        if (!first)
            l.c(' ');
        first = false;
        if (r != 0) {
            l.dec(i).c('!').hex((uint32_t)r);
        } else {
            rel_page(l, s, pa).c('-');
            if (e == 0)
                l.c('z'); // end not reported
            else
                rel_page(l, e, pa);
            l.c(':').hex((uint32_t)t);
        }
        pr = r;
        ps = s;
        pe = e;
        pt = t;
    }
    return l.c(']');
}

static Line& phys_part(Line& l, long pa, int pages) {
    l.s("phys=[");
    for (int i = 0; i < pages; i++)
        l.c(allocated(pa + (long)i * (long)PG) ? 'A' : 'F');
    l.s("] ");
    region_str(l, "gt", pa, pages, true).c(' ');
    return region_str(l, "dq", pa, pages, false);
}

// va maps pages starting at pa_first; released: pages whose backing must not be read (bit i)
static Line& va_part(Line& l, const char* tag, uint8_t* va, long pa_first, int pages,
                     uint32_t stamp_first, bool check_alloc, uint32_t released) {
    l.s(tag).s("=[");
    for (int i = 0; i < pages; i++) {
        uint8_t* p = va + i * PG;
        int32_t prot = 0;
        char c;
        if (!va || !mapped(p, &prot))
            c = '.';
        else if (((released >> i) & 1) ||
                 (check_alloc && !allocated(pa_first + (long)i * (long)PG)))
            c = 'r';
        else if (!(prot & 1))
            c = 'n';
        else
            c = *(volatile uint32_t*)p == stamp_first + (uint32_t)i ? 'M' : 'm';
        l.c(c);
    }
    l.s("] ").s(tag).s("o=[");
    for (int i = 0; i < pages; i++) {
        long ph = -1;
        int32_t t = -1;
        if (va && mapped(va + i * PG, nullptr))
            vq_page(va + i * PG, &ph, &t);
        l.c(ph < 0 ? '-' : ph == pa_first + (long)i * (long)PG ? '=' : '#');
    }
    l.s("] ").s(tag).s("t=[");
    for (int i = 0; i < pages; i++) {
        long ph = -1;
        int32_t t = -1;
        if (va && mapped(va + i * PG, nullptr))
            vq_page(va + i * PG, &ph, &t);
        l.c(t < 0 ? '-' : hexc((uint32_t)t));
    }
    l.s("] ").s(tag).s("p=[");
    for (int i = 0; i < pages; i++) {
        int32_t prot = 0;
        if (va && mapped(va + i * PG, &prot))
            l.c(hexc((uint32_t)prot >> 4)).c(hexc((uint32_t)prot));
        else
            l.s("--");
    }
    return l.c(']');
}

// ---- crash resume -----------------------------------------------------------------------------
// carve.state next to the log: the build, the next entry, the entry running when the app ended,
// entries that ended one launch (run once more) and entries that ended two (skipped from then on).
// A new CP_BUILD starts over and truncates the log; after the last entry the run is not repeated
// until carve.state is deleted.

#define CP_BUILD "carve-1"

struct CpState {
    char build[16];
    int32_t next, inflight;
    uint32_t once[2], bad[2];
};

static void state_save(const CpState* st) {
    const int fd = sceKernelOpen(g_state_path, 0x601, 0777);
    if (fd < 0)
        return;
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}

static bool state_load(CpState* st) {
    const int fd = sceKernelOpen(g_state_path, 0, 0);
    if (fd < 0)
        return false;
    const long n = sceKernelRead(fd, st, sizeof(*st));
    sceKernelClose(fd);
    if (n != (long)sizeof(*st))
        return false;
    for (int i = 0; CP_BUILD[i] || st->build[i]; i++)
        if (i >= 15 || CP_BUILD[i] != st->build[i])
            return false;
    return true;
}

static bool bit(const uint32_t* m, int i) {
    return (m[i >> 5] >> (i & 31)) & 1;
}
static void set_bit(uint32_t* m, int i, bool on) {
    if (on)
        m[i >> 5] |= 1u << (i & 31);
    else
        m[i >> 5] &= ~(1u << (i & 31));
}

// ---- probe definitions ------------------------------------------------------------------------

enum Kind { K_DIRECT, K_ALIAS_GATE, K_FLEX, K_POOL };

// what splits the block's physical range behind A
enum Creator {
    C_NONE,        // nothing
    C_KEEP,        // alias mapped, left mapped
    C_UNMAP,       // alias mapped, then unmapped
    C_MTYPE_SAME,  // alias mapped, then mtypeprotect to type 3 (the block's type)
    C_MTYPE_OTHER, // alias mapped, then mtypeprotect to type 0
    C_MAP2,        // alias mapped with MapDirectMemory2, type 3, left mapped
    C_NAMED_UNMAP, // alias mapped with MapNamedDirectMemory, then unmapped
    C_THREE,       // three one-page aliases mapped, then all three unmapped
    C_POOL_REL1,   // pool: release the second page of the committed block
    C_POOL_RELALL, // pool: release the whole committed block
};

enum Op { O_MTYPE, O_MUNMAP, O_RELEASE, O_CHECKED, O_MPROTECT, O_SPAN, O_DECOMMIT };

struct Spec {
    const char* id;
    const char* name;
    int kind, creator, b_first, b_pages, op, op_first, op_pages;
};

static const Spec SPECS[] = {
    {"C0", "alias before / after EnableDmemAliasing", K_ALIAS_GATE, 0, 0, 0, 0, 0, 0},
    {"C1", "no alias; mtype A 0-7", K_DIRECT, C_NONE, 0, 0, O_MTYPE, 0, 8},
    {"C2", "no alias; munmap A 0-7", K_DIRECT, C_NONE, 0, 0, O_MUNMAP, 0, 8},
    {"C3", "no alias; munmap A 2-4", K_DIRECT, C_NONE, 0, 0, O_MUNMAP, 2, 3},
    {"C4", "no alias; rel 0-7", K_DIRECT, C_NONE, 0, 0, O_RELEASE, 0, 8},
    {"C5", "no alias; chk 0-7", K_DIRECT, C_NONE, 0, 0, O_CHECKED, 0, 8},
    {"C6", "no alias; mtype A 2-4", K_DIRECT, C_NONE, 0, 0, O_MTYPE, 2, 3},
    {"C7", "alias 3-4 kept; munmap A 0-7", K_DIRECT, C_KEEP, 3, 2, O_MUNMAP, 0, 8},
    {"C8", "flexible 4 pages, munmap 1, then 0+2-3", K_FLEX, 0, 0, 0, 0, 0, 0},
    {"C9", "no alias; mtype A+C (A 0-7, C 4p)", K_DIRECT, C_NONE, 0, 0, O_SPAN, 0, 12},
    {"C10", "alias 3-4 unmapped; munmap A 0-1", K_DIRECT, C_UNMAP, 3, 2, O_MUNMAP, 0, 2},
    {"C11", "alias 3-4 unmapped; mtype A 0-1", K_DIRECT, C_UNMAP, 3, 2, O_MTYPE, 0, 2},
    {"C12", "alias 3-4 unmapped; mprotect A 0-7", K_DIRECT, C_UNMAP, 3, 2, O_MPROTECT, 0, 8},
    {"C20", "alias 3-4 unmapped; munmap A 0-7", K_DIRECT, C_UNMAP, 3, 2, O_MUNMAP, 0, 8},
    {"C21", "alias 3-4 mtype 3; munmap A 0-7", K_DIRECT, C_MTYPE_SAME, 3, 2, O_MUNMAP, 0, 8},
    {"C22", "alias 3-4 mtype 0; munmap A 0-7", K_DIRECT, C_MTYPE_OTHER, 3, 2, O_MUNMAP, 0, 8},
    {"C23", "map2 alias 3-4 kept; munmap A 0-7", K_DIRECT, C_MAP2, 3, 2, O_MUNMAP, 0, 8},
    {"C24", "named alias 3-4 unmapped; munmap A", K_DIRECT, C_NAMED_UNMAP, 3, 2, O_MUNMAP, 0, 8},
    {"C25", "alias 0-1 unmapped; munmap A 0-7", K_DIRECT, C_UNMAP, 0, 2, O_MUNMAP, 0, 8},
    {"C26", "alias 6-7 unmapped; munmap A 0-7", K_DIRECT, C_UNMAP, 6, 2, O_MUNMAP, 0, 8},
    {"C27", "aliases 5,6,7 unmapped; munmap A 0-7", K_DIRECT, C_THREE, 5, 3, O_MUNMAP, 0, 8},
    {"C30", "alias 3-4 unmapped; mtype A 0-7", K_DIRECT, C_UNMAP, 3, 2, O_MTYPE, 0, 8},
    {"C31", "alias 3-4 unmapped; munmap A 2-4", K_DIRECT, C_UNMAP, 3, 2, O_MUNMAP, 2, 3},
    {"C32", "alias 3-4 unmapped; rel 0-7", K_DIRECT, C_UNMAP, 3, 2, O_RELEASE, 0, 8},
    {"C33", "alias 3-4 unmapped; chk 0-7", K_DIRECT, C_UNMAP, 3, 2, O_CHECKED, 0, 8},
    {"C34", "alias 3-4 unmapped; mtype A 2-4", K_DIRECT, C_UNMAP, 3, 2, O_MTYPE, 2, 3},
    {"C35", "alias 3-4 unmapped; mtype A+C", K_DIRECT, C_UNMAP, 3, 2, O_SPAN, 0, 12},
    {"C40", "pool commit 64K of 128K; decommit", K_POOL, C_NONE, 0, 0, O_DECOMMIT, 0, 4},
    {"C41", "pool commit, rel page 1; decommit", K_POOL, C_POOL_REL1, 0, 0, O_DECOMMIT, 0, 4},
    {"C42", "pool commit, rel all; decommit", K_POOL, C_POOL_RELALL, 0, 0, O_DECOMMIT, 0, 4},
    {"C43", "pool commit, rel page 1; mtype", K_POOL, C_POOL_REL1, 0, 0, O_MTYPE, 0, 4},
    {"C44", "pool commit, rel page 1; munmap", K_POOL, C_POOL_REL1, 0, 0, O_MUNMAP, 0, 4},
};
static const int NSPECS = (int)(sizeof(SPECS) / sizeof(SPECS[0]));

static uint32_t tag_of(int idx) {
    return 0x43500000u | ((uint32_t)idx << 8);
}

// ---- block: NP test pages between two free pages ----------------------------------------------

struct Block {
    long pa = -1;
    int32_t r_alloc = 0, r_guard_lo = 0, r_guard_hi = 0;
};

static bool block_new(Block& b, int pages) {
    long base = -1;
    b.r_alloc = sceKernelAllocateDirectMemory(0, (long)g_dmem, (pages + 2) * PG, PG, 3, &base);
    if (b.r_alloc != 0)
        return false;
    b.r_guard_lo = sceKernelReleaseDirectMemory(base, PG);
    b.r_guard_hi = sceKernelReleaseDirectMemory(base + (long)(pages + 1) * (long)PG, PG);
    b.pa = base + (long)PG;
    return b.r_guard_lo == 0 && b.r_guard_hi == 0;
}

static uint8_t* map_at(void* fixed, long pa, int pages, int32_t prot, int32_t* r) {
    void* va = fixed;
    *r = sceKernelMapDirectMemory(&va, pages * PG, prot, fixed ? 0x10 : 0, pa, PG);
    return *r == 0 ? (uint8_t*)va : nullptr;
}

struct Alias {
    uint8_t* va = nullptr;
    int first = 0, pages = 0;
};

// ---- direct-memory probes ---------------------------------------------------------------------

static void op_run(Line& l, const Spec& s, uint8_t* a, long pa, uint8_t* span, int32_t prot) {
    uint8_t* at = a + s.op_first * PG;
    const uint64_t len = (uint64_t)s.op_pages * PG;
    int32_t r = 0;
    switch (s.op) {
    case O_MTYPE:
        r = sceKernelMtypeprotect(at, len, 3, prot);
        break;
    case O_MUNMAP:
        r = sceKernelMunmap(at, len);
        break;
    case O_RELEASE:
        r = sceKernelReleaseDirectMemory(pa + s.op_first * (long)PG, len);
        break;
    case O_CHECKED:
        r = sceKernelCheckedReleaseDirectMemory(pa + s.op_first * (long)PG, len);
        break;
    case O_MPROTECT:
        r = sceKernelMprotect(at, len, 0x1);
        break;
    case O_SPAN:
        r = sceKernelMtypeprotect(span, len, 3, prot);
        break;
    }
    l.ret("op", r);
}

static Line& aliases_part(Line& l, const Alias* al, int n, long pa, uint32_t tag) {
    for (int k = 0; k < n; k++) {
        char name[3] = {'B', 0, 0};
        if (k)
            name[1] = (char)('1' + k);
        l.s(" | ");
        va_part(l, name, al[k].va, pa + al[k].first * (long)PG, al[k].pages,
                tag + (uint32_t)al[k].first, true, 0);
    }
    return l;
}

static void run_direct(int idx) {
    const Spec& s = SPECS[idx];
    const uint32_t tag = tag_of(idx);
    const bool span = s.op == O_SPAN;
    const int32_t prot = span ? 0x33 : 0x3;

    // span: C's block first, so P's guard pages stay free
    Block cblk;
    if (span && !block_new(cblk, 4)) {
        Line l = head(s.id, s.name);
        l.s("SKIP block C ").ret("alloc", cblk.r_alloc).ret("lo", cblk.r_guard_lo);
        l.ret("hi", cblk.r_guard_hi).end();
        if (cblk.pa > 0)
            release_pages(cblk.pa, 4);
        return;
    }
    Block blk;
    if (!block_new(blk, NP)) {
        Line l = head(s.id, s.name);
        l.s("SKIP block ").ret("alloc", blk.r_alloc).ret("lo", blk.r_guard_lo);
        l.ret("hi", blk.r_guard_hi).end();
        if (blk.pa > 0)
            release_pages(blk.pa, NP);
        if (span)
            release_pages(cblk.pa, 4);
        return;
    }

    // span: A 0-7 and C (the second block) side by side in one reservation
    uint8_t* rsv = nullptr;
    const long cpa = span ? cblk.pa : -1;
    uint8_t* c = nullptr;
    int32_t r_rsv = 0, r_c = 0;
    if (span) {
        void* v = nullptr;
        r_rsv = sceKernelReserveVirtualRange(&v, (NP + 4) * PG, 0, PG);
        rsv = r_rsv == 0 ? (uint8_t*)v : nullptr;
    }

    int32_t r_a = 0;
    uint8_t* a = (span && !rsv) ? nullptr : map_at(rsv, blk.pa, NP, prot, &r_a);
    if (span && a && cpa >= 0)
        c = map_at(rsv + NP * PG, cpa, 4, prot, &r_c);
    if (!a || (span && !c)) {
        Line l = head(s.id, s.name);
        l.s("SKIP setup ").ret("mapA", r_a);
        if (span)
            l.ret("rsv", r_rsv).ret("mapC", r_c);
        l.end();
        unmap_pages(a, NP);
        unmap_pages(c, 4);
        if (rsv)
            sceKernelMunmap(rsv, (NP + 4) * PG);
        release_pages(blk.pa, NP);
        release_pages(cpa, 4);
        return;
    }
    stamp(a, NP, tag);
    if (c)
        stamp(c, 4, tag + 0x80);

    // creator
    Alias al[3];
    int nal = 0;
    Line pre = head(s.id, s.name);
    pre.s("pre ");
    const long bpa = blk.pa + s.b_first * (long)PG;
    switch (s.creator) {
    case C_NONE:
        break;
    case C_KEEP:
    case C_UNMAP:
    case C_MTYPE_SAME:
    case C_MTYPE_OTHER: {
        int32_t r = 0;
        al[0].va = map_at(nullptr, bpa, s.b_pages, 0x3, &r);
        al[0].first = s.b_first;
        al[0].pages = s.b_pages;
        nal = 1;
        pre.ret("mapB", r);
        if (al[0].va && s.creator == C_UNMAP)
            pre.ret("unmapB", sceKernelMunmap(al[0].va, s.b_pages * PG));
        else if (al[0].va && s.creator == C_MTYPE_SAME)
            pre.ret("mtypeB", sceKernelMtypeprotect(al[0].va, s.b_pages * PG, 3, 0x3));
        else if (al[0].va && s.creator == C_MTYPE_OTHER)
            pre.ret("mtypeB", sceKernelMtypeprotect(al[0].va, s.b_pages * PG, 0, 0x3));
        break;
    }
    case C_MAP2: {
        void* v = nullptr;
        const int32_t r = sceKernelMapDirectMemory2(&v, s.b_pages * PG, 3, 0x3, 0, bpa, PG);
        al[0].va = r == 0 ? (uint8_t*)v : nullptr;
        al[0].first = s.b_first;
        al[0].pages = s.b_pages;
        nal = 1;
        pre.ret("map2B", r);
        break;
    }
    case C_NAMED_UNMAP: {
        void* v = nullptr;
        const int32_t r =
            sceKernelMapNamedDirectMemory(&v, s.b_pages * PG, 0x3, 0, bpa, PG, "carve");
        al[0].va = r == 0 ? (uint8_t*)v : nullptr;
        al[0].first = s.b_first;
        al[0].pages = s.b_pages;
        nal = 1;
        pre.ret("namedB", r);
        if (al[0].va)
            pre.ret("unmapB", sceKernelMunmap(al[0].va, s.b_pages * PG));
        break;
    }
    case C_THREE: {
        static const char* const map_tag[3] = {"mapB", "mapB2", "mapB3"};
        static const char* const unmap_tag[3] = {"unmapB", "unmapB2", "unmapB3"};
        for (int k = 0; k < 3; k++) {
            int32_t r = 0;
            al[k].va = map_at(nullptr, bpa + k * (long)PG, 1, 0x3, &r);
            al[k].first = s.b_first + k;
            al[k].pages = 1;
            pre.ret(map_tag[k], r);
        }
        nal = 3;
        for (int k = 0; k < 3; k++)
            if (al[k].va)
                pre.ret(unmap_tag[k], sceKernelMunmap(al[k].va, PG));
        break;
    }
    }
    pre.s("| ");
    phys_part(pre, blk.pa, NP).s(" | ");
    va_part(pre, "A", a, blk.pa, NP, tag, true, 0);
    aliases_part(pre, al, nal, blk.pa, tag);
    if (c) {
        pre.s(" | ");
        va_part(pre, "C", c, cpa, 4, tag + 0x80, true, 0);
    }
    pre.end();

    Line post = head(s.id, s.name);
    op_run(post, s, a, blk.pa, rsv, prot);
    post.s("| ");
    phys_part(post, blk.pa, NP).s(" | ");
    va_part(post, "A", a, blk.pa, NP, tag, true, 0);
    aliases_part(post, al, nal, blk.pa, tag);
    if (c) {
        post.s(" | ");
        va_part(post, "C", c, cpa, 4, tag + 0x80, true, 0);
        post.s(" ");
        phys_part(post, cpa, 4);
    }
    post.end();

    unmap_pages(a, NP);
    for (int k = 0; k < nal; k++)
        unmap_pages(al[k].va, al[k].pages);
    unmap_pages(c, 4);
    if (rsv)
        sceKernelMunmap(rsv, (NP + 4) * PG);
    release_pages(blk.pa, NP);
    release_pages(cpa, 4);
}

// ---- C0: can the same memory be mapped twice, before and after enabling aliasing -------------

static void run_alias_gate(int idx) {
    const Spec& s = SPECS[idx];
    const uint32_t tag = tag_of(idx);
    long pa = -1;
    const int32_t ra = sceKernelAllocateDirectMemory(0, (long)g_dmem, 2 * PG, PG, 3, &pa);
    Line l = head(s.id, s.name);
    if (ra != 0) {
        l.s("SKIP ").ret("alloc", ra).ret("enable", sceKernelEnableDmemAliasing()).end();
        return;
    }
    int32_t r1 = 0, r2 = 0, r3 = 0;
    uint8_t* v1 = map_at(nullptr, pa, 2, 0x3, &r1);
    if (v1)
        stamp(v1, 2, tag);
    uint8_t* v2 = map_at(nullptr, pa, 2, 0x3, &r2);
    const int32_t re = sceKernelEnableDmemAliasing();
    uint8_t* v3 = map_at(nullptr, pa, 2, 0x3, &r3);
    l.ret("map1", r1).ret("before", r2).ret("enable", re).ret("after", r3).s("| ");
    va_part(l, "V1", v1, pa, 2, tag, true, 0).s(" | ");
    va_part(l, "V2", v2, pa, 2, tag, true, 0).s(" | ");
    va_part(l, "V3", v3, pa, 2, tag, true, 0).end();
    unmap_pages(v1, 2);
    unmap_pages(v2, 2);
    unmap_pages(v3, 2);
    release_pages(pa, 2);
}

// ---- C8: flexible memory, the other branch of the physical-area unmap -------------------------

static void run_flex(int idx) {
    const Spec& s = SPECS[idx];
    const uint32_t tag = tag_of(idx);
    void* v = nullptr;
    const int32_t rm = sceKernelMapFlexibleMemory(&v, 4 * PG, 0x3, 0);
    uint8_t* f = rm == 0 ? (uint8_t*)v : nullptr;
    if (!f) {
        Line l = head(s.id, s.name);
        l.s("SKIP ").ret("map", rm).end();
        return;
    }
    stamp(f, 4, tag);
    Line pre = head(s.id, s.name);
    pre.s("pre ").ret("map", rm).ret("unmap1", sceKernelMunmap(f + PG, PG)).s("| ");
    va_part(pre, "F", f, -1, 4, tag, false, 0).end();
    Line post = head(s.id, s.name);
    post.ret("unmap0", sceKernelMunmap(f, PG)).ret("unmap23", sceKernelMunmap(f + 2 * PG, 2 * PG));
    post.s("| ");
    va_part(post, "F", f, -1, 4, tag, false, 0).end();
    unmap_pages(f, 4);
}

// ---- C40-C44: pool -----------------------------------------------------------------------------

static void run_pool(int idx) {
    const Spec& s = SPECS[idx];
    const uint32_t tag = tag_of(idx);
    const uint64_t blk = 0x10000, rsv = 0x200000;
    const int pages = (int)(blk / PG);
    uint64_t pp = 0;
    const int32_t re = sceKernelMemoryPoolExpand(0, g_dmem, 2 * blk, blk, &pp);
    void* v = nullptr;
    const int32_t rr = re == 0 ? sceKernelMemoryPoolReserve(0, rsv, 0, 0, &v) : -1;
    const int32_t rc = rr == 0 ? sceKernelMemoryPoolCommit(v, blk, 3, 0x3, 0) : -1;
    uint8_t* va = rc == 0 ? (uint8_t*)v : nullptr;
    if (!va) {
        Line l = head(s.id, s.name);
        l.s("SKIP ").ret("expand", re).ret("reserve", rr).ret("commit", rc).end();
        if (rr == 0)
            sceKernelMunmap(v, rsv);
        if (re == 0)
            sceKernelReleaseDirectMemory((long)pp, 2 * blk);
        return;
    }
    stamp(va, pages, tag);
    const long pa = (long)pp;

    Line pre = head(s.id, s.name);
    pre.s("pre ").ret("expand", re).ret("reserve", rr).ret("commit", rc);
    uint32_t released = 0;
    if (s.creator == C_POOL_REL1) {
        pre.ret("rel1", sceKernelReleaseDirectMemory(pa + (long)PG, PG));
        released = 0x2;
    } else if (s.creator == C_POOL_RELALL) {
        pre.ret("relall", sceKernelReleaseDirectMemory(pa, blk));
        released = 0xF;
    }
    pre.s("| ");
    phys_part(pre, pa, pages).s(" | ");
    va_part(pre, "V", va, pa, pages, tag, false, released).end();

    Line post = head(s.id, s.name);
    int32_t ro = 0;
    if (s.op == O_DECOMMIT)
        ro = sceKernelMemoryPoolDecommit(va, blk, 0);
    else if (s.op == O_MTYPE)
        ro = sceKernelMtypeprotect(va, blk, 3, 0x3);
    else
        ro = sceKernelMunmap(va, blk);
    post.ret("op", ro).s("| ");
    phys_part(post, pa, pages).s(" | ");
    va_part(post, "V", va, pa, pages, tag, false, released).end();

    if (s.op == O_MTYPE)
        sceKernelMemoryPoolDecommit(va, blk, 0);
    sceKernelMunmap(va, rsv);
    sceKernelReleaseDirectMemory(pa, 2 * blk);
}

// ---- main -------------------------------------------------------------------------------------

static void run_one(int idx) {
    switch (SPECS[idx].kind) {
    case K_ALIAS_GATE:
        run_alias_gate(idx);
        break;
    case K_FLEX:
        run_flex(idx);
        break;
    case K_POOL:
        run_pool(idx);
        break;
    default:
        run_direct(idx);
        break;
    }
}

int main(void) {
    g_screen.init();
    log_pick_dir();
    g_dmem = (uint64_t)sceKernelGetDirectMemorySize();

    CpState st;
    my_memset(&st, 0, sizeof(st));
    const bool resumed = state_load(&st);
    if (!resumed) {
        my_memset(&st, 0, sizeof(st));
        for (int i = 0; CP_BUILD[i] && i < 15; i++)
            st.build[i] = CP_BUILD[i];
        st.next = 0;
        st.inflight = -1;
        log_truncate();
        state_save(&st);
    }

    say("===== CARVE TEST =====");
    {
        Line l;
        l.s("build=").s(CP_BUILD).s(" dmem size=").hex(g_dmem).s(" page=").hex(PG);
        l.s(resumed ? " launch=resumed" : " launch=fresh");
        l.s(" next=").s(st.next < NSPECS ? SPECS[st.next].id : "done").end();
    }
    if (!resumed)
        say("phys: A alloc F free | va: M ok m changed n no-read r backing freed . unmapped | "
            "o: = expected phys # other - none");

    if (st.inflight >= 0 && st.inflight < NSPECS) {
        const int i = st.inflight;
        Line l = head(SPECS[i].id, SPECS[i].name);
        if (bit(st.once, i)) {
            set_bit(st.bad, i, true);
            l.s("ENDED the last launch again: skipped from now on").end();
        } else {
            set_bit(st.once, i, true);
            l.s("ENDED the last launch: running it once more").end();
        }
        st.next = i;
        st.inflight = -1;
        state_save(&st);
    }

    if (st.next >= NSPECS) {
        say("all entries done earlier; delete carve.state to run them again");
    } else {
        if (st.next > 0) {
            Line l;
            l.s("aliasing for this launch ").ret("enable", sceKernelEnableDmemAliasing()).end();
        }
        for (int i = st.next; i < NSPECS; i++) {
            if (bit(st.bad, i)) {
                Line l = head(SPECS[i].id, SPECS[i].name);
                l.s("SKIP ended two launches").end();
                st.next = i + 1;
                state_save(&st);
                continue;
            }
            st.inflight = i;
            state_save(&st);
            run_one(i);
            set_bit(st.once, i, false);
            st.inflight = -1;
            st.next = i + 1;
            state_save(&st);
        }
        say("===== DONE =====");
    }
    for (;;)
        sceKernelUsleep(1000000);
    return 0;
}
