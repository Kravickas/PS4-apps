// The SDK versions of the SDK 10+ package (make SDK10=1), in order: eboot.bin at the first one, the
// others as sdk/eboot_<version>.bin (12.50 is newer than the console's FW 12.02). A package of their
// own: FW 12.02 refuses an executable whose modules declare an older SDK than 10.00 (OpenOrbis'
// declare 1.00) or a newer one than the executable, and all executables of a package share them.
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
