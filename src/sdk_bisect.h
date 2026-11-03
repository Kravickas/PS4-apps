// SDK bisect (make BISECT=1): the process SDK versions the bisect package runs at, in order. The full
// chain (sdk_chain.h) found rules changing at 2.00, 2.50, 3.00, 3.50 and 5.00; this list has every
// released firmware version from 1.50 to 3.50 and from 4.50 to 5.00 (PS4 firmware release list), so
// each change is placed to the released version. The last entry tries SDK 10.00 with the
// executable's firmware version field set (FW 12.02 refused it as corrupted without). eboot.bin is built at the first one (the Makefile
// reads this list).
#pragma once
#include <stdint.h>
static const uint32_t g_chain_sdks[] = {
    0x01508001,
    0x01518001,
    0x01528001,
    0x01608001,
    0x01618001,
    0x01628001,
    0x01708001,
    0x01718001,
    0x01728001,
    0x01748001,
    0x01758001,
    0x01768001,
    0x02008001,
    0x02018001,
    0x02028001,
    0x02038001,
    0x02048001,
    0x02508001,
    0x02518001,
    0x02558001,
    0x02578001,
    0x03008001,
    0x03108001,
    0x03118001,
    0x03158001,
    0x03508001,
    0x04508001,
    0x04558001,
    0x04708001,
    0x04718001,
    0x04728001,
    0x04738001,
    0x04748001,
    0x05008001,
    // last: SDK 10.00 with one change, -fwversion set to the SDK version (see the Makefile)
    0x10008001,
};
static const int g_chain_n = (int)(sizeof(g_chain_sdks) / sizeof(g_chain_sdks[0]));
