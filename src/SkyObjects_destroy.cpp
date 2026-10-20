// CSkyObjects::DestroySkyObjects (main app, SimTheme Park 2.04b3, Text.cpp's range in the inventory; unit w81).
#include "SkyObjects.h"

#include "MainBalance.h"
#include "tb_sky.h"

// 0x1015b804 (116 bytes). One RemoveAdd(void*) on the engine's sky (gei_Sky, an import) for each of the first NumSkyObjects pointers.
// The count (gMainBalance value slot 0x6e4, a signed compare) and the pointer are read again on every iteration, as on the Mac.
void CSkyObjects::DestroySkyObjects() {
    for (int32_t i = 0; i < gMainBalance.mValues[kMainBalanceNumSkyObjects].mInteger; ++i) {
        gei_Sky.RemoveAdd(mObjects[i]);
    }
}
