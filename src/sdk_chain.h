// The process SDK versions DmemTest runs at, in order, in the release format (major, minor, then
// patch 001; 5.05 is written 05050001): SDK 1.00 (eboot.bin) and every released version at which a
// rule changes on FW 12.02 (1.70, 2.00, 2.50, 3.00, 3.50, 5.00), found by running every released
// version from 1.00 to 9.50 (analysis/VERIFY.md), then 10.00. The Makefile reads this list (only the
// values below may be written as hex here): every executable gets its version in its process
// parameter (create-fself -sdkver), and param.sfo declares the last one.
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x01000001,
    0x01700001,
    0x02000001,
    0x02500001,
    0x03000001,
    0x03500001,
    0x05000001,
    0x10000001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
