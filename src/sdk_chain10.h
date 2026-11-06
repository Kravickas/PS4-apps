// The SDK versions from 10.00 that get a package each (make SDK10=1 SDK=<version>; CI builds one per
// line; 12.50 is newer than the console's FW 12.02). One package per version: FW 12.02 refuses an
// executable from 10.00 whose modules declare another SDK than it, and a package shares its modules.
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x10008001,
    0x10508001,
    0x11008001,
    0x11508001,
    0x12008001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
