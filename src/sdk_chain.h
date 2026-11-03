// The process SDK versions DmemTest runs at, in order: the first is create-fself's default (the
// package's eboot.bin), the others are built as sdk/eboot_0xXXXXXXXX.bin (the Makefile reads this
// list). Every major SDK version, 1.00 to 9.50 in steps of 0.50. 10.00 and above are left out: on
// FW 12.02 the system refused the 10.00 executable as corrupted (chain run after 9.50).
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x01000051,
    0x01508001,
    0x02008001,
    0x02508001,
    0x03008001,
    0x03508001,
    0x04008001,
    0x04508001,
    0x05008001,
    0x05508001,
    0x06008001,
    0x06508001,
    0x07008001,
    0x07508001,
    0x08008001,
    0x08508001,
    0x09008001,
    0x09508001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
