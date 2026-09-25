/* bmp_loader.h — Load BMP texture into GPU memory as RGBA8 */

typedef struct {
    void *pixels;      /* RGBA8 pixel data (GPU allocated) */
    int width, height;
    unsigned long size; /* total bytes */
} BmpTexture;

/* Row scratch buffer in ordinary cached process memory. It used to come from
   alloc_fn, i.e. GARLIC for GPU textures, and the conversion loop read it back
   byte by byte - uncached CPU reads, ~3 per pixel. */
#define BMP_MAX_W 8192
static unsigned char g_bmp_row[BMP_MAX_W * 4 + 16];

static int bmp_load(const char *path, void *(*alloc_fn)(unsigned long, unsigned long), BmpTexture *out) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return -1;

    /* Read BMP header (14 bytes) + DIB header (40 bytes) */
    unsigned char hdr[54];
    if (sceKernelRead(fd, hdr, 54) < 54) { sceKernelClose(fd); return -2; }

    if (hdr[0] != 'B' || hdr[1] != 'M') { sceKernelClose(fd); return -3; }

    unsigned int data_off = *(unsigned int*)(hdr + 10);
    int w = *(int*)(hdr + 18);
    int h = *(int*)(hdr + 22);
    int bpp = *(unsigned short*)(hdr + 28);
    int compr = *(unsigned int*)(hdr + 30);
    int flip = 1; /* BMP is bottom-up by default */
    if (h < 0) { h = -h; flip = 0; } /* top-down BMP */

    if (compr != 0 || (bpp != 24 && bpp != 32)) { sceKernelClose(fd); return -4; }
    if (w <= 0 || w > BMP_MAX_W || h <= 0) { sceKernelClose(fd); return -7; }

    /* Allocate RGBA8 output */
    unsigned long out_size = (unsigned long)w * h * 4;
    void *pixels = alloc_fn(out_size, 0x1000);
    if (!pixels) { sceKernelClose(fd); return -5; }

    /* Read pixel data */
    sceKernelLseek(fd, data_off, 0);
    int stride_bmp = ((w * (bpp / 8) + 3) & ~3); /* BMP rows are 4-byte aligned */
    unsigned long row_sz = stride_bmp;

    /* Read row by row, convert BGR(A) → RGBA, flip if needed */
    /* Allocate row buffer on heap for large textures */
    unsigned char *row_buf = g_bmp_row;
    unsigned char *dst = (unsigned char*)pixels;

    for (int y = 0; y < h; y++) {
        int src_y = y;  /* no flip: bottom-up */
        sceKernelLseek(fd, data_off + (long)src_y * stride_bmp, 0);
        int rd = sceKernelRead(fd, row_buf, row_sz);
        if (rd < (int)(w * (bpp / 8))) break;

        unsigned char *d = dst + (long)y * w * 4;
        for (int x = 0; x < w; x++) {
            int sx = x; /* no flip — correct UV mapping */
            if (bpp == 24) {
                d[x*4+0] = row_buf[sx*3+2]; /* R */
                d[x*4+1] = row_buf[sx*3+1]; /* G */
                d[x*4+2] = row_buf[sx*3+0]; /* B */
                d[x*4+3] = 255;             /* A */
            } else { /* 32bpp BGRA */
                d[x*4+0] = row_buf[sx*4+2]; /* R */
                d[x*4+1] = row_buf[sx*4+1]; /* G */
                d[x*4+2] = row_buf[sx*4+0]; /* B */
                d[x*4+3] = row_buf[sx*4+3]; /* A */
            }
        }
    }
    sceKernelClose(fd);

    out->pixels = pixels;
    out->width = w;
    out->height = h;
    out->size = out_size;
    return 0;
}
