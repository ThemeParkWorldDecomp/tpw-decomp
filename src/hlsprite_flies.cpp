// HLSprite.cpp range of the main app (SimTheme Park 2.04b3): the flies' shutdown. FLY_Init (0x100752d4, untranslated) makes the
// sine table and the bases; the globals are defined here with the first function that uses them.
#include "HLSprite.h"

#include "LbFile.h"

int32_t gFlyActive = 0;           // 0x102daaf4
int32_t gFlyInitialised = 0;      // 0x102daaf8
void* gFlySinTable = nullptr;     // 0x102daafc
void* gFlyBases[100] = {};        // 0x102da964

// Frees the sine table and removes the bases 1 to 99 (not 0), when FLY_Init has run; then FLY_Init may run again.
void FLY_Close() {
    if (gFlyInitialised != 0) {
        LbMem_Free(gFlySinTable);
        gFlySinTable = nullptr;
        for (int32_t i = 1; i < 100; ++i) FLY_RemoveBase(i);
        gFlyInitialised = 0;
    }
}
