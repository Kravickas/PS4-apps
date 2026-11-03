// The process SDK versions DmemTest runs at, in order: the first is create-fself's default (the
// package's eboot.bin), the others are built as sdk/eboot_0xXXXXXXXX.bin (the Makefile reads this
// list). Every major SDK version, 1.00 to 12.00 in steps of 0.50.
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
    0x10008001,
    0x10508001,
    0x11008001,
    0x11508001,
    0x12008001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
