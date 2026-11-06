// The process SDK versions DmemTest runs at, in order: SDK 1.00 (eboot.bin, create-fself's default)
// and every released version at which a rule changes on FW 12.02 (1.70, 2.00, 2.50, 3.00, 3.50, 5.00),
// found by running every released version from 1.00 to 9.50 (analysis/VERIFY.md). The others are
// built as sdk/eboot_<version>.bin; the Makefile reads this list. SDK 10.00 and above are a package
// of their own (sdk_chain10.h, make SDK10=1).
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x01000051,
    0x01708001,
    0x02008001,
    0x02508001,
    0x03008001,
    0x03508001,
    0x05008001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
