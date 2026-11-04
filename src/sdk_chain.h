// The process SDK versions DmemTest runs at, in order. A PS4 SDK version is major.minor.patch with a
// three-digit minor and patch, stored digit for digit (11.008.001 is written 11008001, as in Gran
// Turismo 7's executable; the released SDK x.y0 is x.y08.001). SDK 1.00 (eboot.bin) and every released
// version at which a rule changes on FW 12.02 (1.70, 2.00, 2.50, 3.00, 3.50, 5.00), found by running
// every released version from 1.00 to 9.50 (analysis/VERIFY.md), then the SDK versions 10.00 to
// 12.00 (12.50 is newer than the console's FW 12.02). The Makefile reads this
// list (only the values below may be written as hex here): every executable gets its version in its
// process parameter (create-fself -sdkver), and param.sfo declares the last one.
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x01008001,
    0x01708001,
    0x02008001,
    0x02508001,
    0x03008001,
    0x03508001,
    0x05008001,
    0x10008001,
    0x10508001,
    0x11008001,
    0x11508001,
    0x12008001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
