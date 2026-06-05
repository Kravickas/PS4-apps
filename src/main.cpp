/*
 * PS4 FP Table Dumper - Minimal homebrew, no libc
 * Dumps RCPSS/RSQRTSS hardware lookup tables from AMD Jaguar
 * Output: /data/jaguar_fp_tables.h (retrieve via FTP)
 */

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint64_t;
typedef long           int64_t;
typedef int            int32_t;
typedef unsigned char  uint8_t;
typedef unsigned long  size_t;

/* PS4 kernel imports - C linkage guard for C++ compilation */
#ifdef __cplusplus
extern "C" {
#endif
int sceKernelOpen(const char *path, int flags, int mode);
int64_t sceKernelWrite(int fd, const void *buf, size_t nbyte);
int sceKernelClose(int fd);
void _exit(int status);
#ifdef __cplusplus
}
#endif

/* O_WRONLY|O_CREAT|O_TRUNC = 0x601 on FreeBSD */
#define O_FILE_CREATE 0x601

/* SSE via inline asm. Use "x" constraint (XMM reg) so the compiler
   allocates XMM registers directly -- avoids the movd GPR<->XMM sizing
   ambiguity that occurs when "r" picks a 64-bit GPR on x86-64. */
static uint32_t do_rcpss(uint32_t raw) {
    float fin, fout;
    __builtin_memcpy(&fin, &raw, 4);
    __asm__ volatile("rcpss %1, %0" : "=x"(fout) : "x"(fin));
    uint32_t result;
    __builtin_memcpy(&result, &fout, 4);
    return result;
}

static uint32_t do_rsqrtss(uint32_t raw) {
    float fin, fout;
    __builtin_memcpy(&fin, &raw, 4);
    __asm__ volatile("rsqrtss %1, %0" : "=x"(fout) : "x"(fin));
    uint32_t result;
    __builtin_memcpy(&result, &fout, 4);
    return result;
}

/* Mini number-to-hex without libc */
static void u32_to_hex(uint32_t v, char *buf, int digits) {
    const char *hex = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
}

static int my_strlen(const char *s) {
    int n = 0;
    while (*s++) n++;
    return n;
}

static void my_write_str(int fd, const char *s) {
    sceKernelWrite(fd, s, my_strlen(s));
}

static void write_u16_hex(int fd, uint16_t val) {
    char buf[6];
    buf[0] = '0'; buf[1] = 'x';
    u32_to_hex(val, buf + 2, 3);
    buf[5] = '\0';
    my_write_str(fd, buf);
}

static void write_u32_hex(int fd, uint32_t val) {
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    u32_to_hex(val, buf + 2, 8);
    buf[10] = '\0';
    my_write_str(fd, buf);
}

int main(void) {
    /* Generate tables */
    uint16_t rcpss_table[2048];
    uint16_t rsqrtss_table[2048];

    /* RCPSS: test with exp=127, mantissa_top_11 = i */
    for (int i = 0; i < 2048; i++) {
        uint32_t input = (127u << 23) | ((uint32_t)i << 12);
        uint32_t output = do_rcpss(input);
        rcpss_table[i] = (uint16_t)((output >> 11) & 0xFFF);
    }

    /* RSQRTSS: even exp (126) for entries 0..1023, odd exp (127) for 1024..2047 */
    for (int i = 0; i < 1024; i++) {
        uint32_t input = (126u << 23) | ((uint32_t)i << 13);
        uint32_t output = do_rsqrtss(input);
        rsqrtss_table[i] = (uint16_t)((output >> 11) & 0xFFF);
    }
    for (int i = 0; i < 1024; i++) {
        uint32_t input = (127u << 23) | ((uint32_t)i << 13);
        uint32_t output = do_rsqrtss(input);
        rsqrtss_table[1024 + i] = (uint16_t)((output >> 11) & 0xFFF);
    }

    /* Verification values */
    uint32_t rcp_one = do_rcpss(0x3F800000);      /* RCPSS(1.0) */
    uint32_t rsqrt_four = do_rsqrtss(0x40800000);  /* RSQRTSS(4.0) */
    uint32_t rsqrt_256 = do_rsqrtss(0x43800000);   /* RSQRTSS(256.0) */

    /* Write to file */
    int fd = sceKernelOpen("/data/jaguar_fp_tables.h", O_FILE_CREATE, 0644);
    if (fd < 0) {
        /* Try current dir as fallback */
        fd = sceKernelOpen("jaguar_fp_tables.h", O_FILE_CREATE, 0644);
    }
    if (fd < 0) {
        _exit(1);
    }

    my_write_str(fd, "// Auto-generated on PS4 (AMD Jaguar)\n");
    my_write_str(fd, "// Verification: RCPSS(1.0)=");
    write_u32_hex(fd, rcp_one);
    my_write_str(fd, " RSQRTSS(4.0)=");
    write_u32_hex(fd, rsqrt_four);
    my_write_str(fd, " RSQRTSS(256)=");
    write_u32_hex(fd, rsqrt_256);
    my_write_str(fd, "\n\n#pragma once\n\n#include <cstdint>\n\nnamespace Core {\n\n");

    /* RCPSS table */
    my_write_str(fd, "static constexpr uint16_t kJaguarRcpTable[2048] = {\n");
    for (int i = 0; i < 2048; i++) {
        if (i % 16 == 0) my_write_str(fd, "    ");
        write_u16_hex(fd, rcpss_table[i]);
        if (i < 2047) my_write_str(fd, ",");
        if (i % 16 == 15) my_write_str(fd, "\n");
    }
    my_write_str(fd, "};\n\n");

    /* RSQRTSS table */
    my_write_str(fd, "static constexpr uint16_t kJaguarRsqrtTable[2048] = {\n");
    for (int i = 0; i < 2048; i++) {
        if (i % 16 == 0) my_write_str(fd, "    ");
        write_u16_hex(fd, rsqrtss_table[i]);
        if (i < 2047) my_write_str(fd, ",");
        if (i % 16 == 15) my_write_str(fd, "\n");
    }
    my_write_str(fd, "};\n\n} // namespace Core\n");

    sceKernelClose(fd);
    _exit(0);
    return 0;
}
