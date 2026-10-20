#include "VanillaTime.h"
#include "LbFile.h"
#include "LbTime.h"
#include "platform/samsutils.h"

// __sinit_VanillaTime_cpp at 0x10168830, object at 0x10380a04.
CVanillaTime gVanillaTime;

// The Mac swaps these words around file I/O: the on-disk bytes are little-endian.
// Native values on a little-endian host produce the same file bytes.
bool CVanillaTime::Save(void* file) {
    uint32_t clock = LbTime_GetClock();
    uint32_t transferred;
    LbFile_Write(file, &clock, sizeof clock, &transferred);
    bool success = (transferred >> 2) == 1;
    if (!success) SamsUtilities::UDebug("write failed");
    return success;
}

bool CVanillaTime::Load(void* file) {
    uint32_t clock, transferred;
    LbFile_Read(file, &clock, sizeof clock, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return false;
    }
    mTimeOffset = clock - LbTime_GetClock();
    return true;
}
