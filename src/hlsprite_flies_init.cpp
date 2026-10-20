// HLSprite.cpp range of the main app (SimTheme Park 2.04b3): FLY_Init (0x100752d4, 244 bytes), the start of the flies. The
// globals are in src/hlsprite_flies.cpp.
#include <cmath>

#include "HLSprite.h"
#include "LbFile.h"
#include "ppc_math.h"

// The first call (gFlyInitialised is 0) marks the flies running, empties the 100 base slots and makes the table of sines the
// flies' wings flap by: 4,096 words, entry i = (int)(1024 * sin(PI * 2i / 4096) + 0.5), with the game's own PI of 3.14159
// (not the exact one), so the table is 1024 sin(i * 2 PI' / 4096) rounded to the nearest. Later calls (before FLY_Close) do
// nothing. The multiply and add of the entry is one fused fmadd on the Mac.
void FLY_Init() {
    if (gFlyInitialised != 0) return;
    gFlyInitialised = 1;
    gFlyActive = 1;
    for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = nullptr;
    gFlySinTable = LbMem_Alloc(0x4000);
    int32_t* table = static_cast<int32_t*>(gFlySinTable);
    for (int32_t i = 0; i < 0x1000; ++i) {
        const double angle = (3.14159 * static_cast<double>(i << 1)) / 4096.0;
        table[i] = ppc_fctiwz(ppc_fmadd(1024.0, std::sin(angle), 0.5));
    }
}
