#include "AudioPlaceHolder.h"

// LbMaths_Random__FRl 0x10018cf4 (80): linear congruential step; the RotateRight result is dropped by the original.
int32_t LbMaths_Random(int32_t& seed) {
    const int32_t next = static_cast<int32_t>(static_cast<uint32_t>(seed) * 0x19660du + 0x3c6ef35fu);
    seed = next;
    SamsUtilities::UBinary::RotateRight<int32_t>(seed, 13);
    return next;
}
