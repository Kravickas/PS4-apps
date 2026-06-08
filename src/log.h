#pragma once
#include <stdint.h>
#include "myprintf.h"

extern "C" {
    int sceKernelOpen(const char* path, int flags, unsigned short mode);
    long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int sceKernelClose(int fd);
    int sceKernelFsync(int fd);
}

static int g_log_fd = -1;

static void log_init() {
    g_log_fd = sceKernelOpen("/temp0/results.txt", 0x0601, 0777);
    if (g_log_fd < 0)
        g_log_fd = sceKernelOpen("/data/results.txt", 0x0601, 0777);
}

static void log_flush() {
    if (g_log_fd >= 0) sceKernelFsync(g_log_fd);
}

static void log_close() {
    log_flush();
    if (g_log_fd >= 0) sceKernelClose(g_log_fd);
}

static void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf(const char* fmt, ...) {
    char buf[512];
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int len = my_vsnprintf(buf, sizeof(buf), fmt, ap);
    __builtin_va_end(ap);
    if (len > 0 && g_log_fd >= 0)
        sceKernelWrite(g_log_fd, buf, (unsigned long)(len < 512 ? len : 511));
}
