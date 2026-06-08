#pragma once
#include <stdint.h>
#include <stddef.h>

typedef long off_t;

// Minimal PS4 kernel API declarations
extern "C" {
    int32_t sceKernelAllocateDirectMemory(
        long search_start, long search_end, size_t len,
        size_t alignment, int memory_type, long* phys_addr_out);
    int32_t sceKernelMapDirectMemory(
        void** addr, size_t len, int prot, int flags,
        long phys_addr, size_t alignment);
    int sceKernelUsleep(unsigned int usec);
}
