// keyboard.cpp cheats: place_litter 0x1014dac8 (136 bytes) and place_bomb 0x1014db74 (92 bytes).
// Both put litter on the cell whose number is the interface's mF24 (the cells start at +0x2d8 of the world, 0x44 bytes each).
#include <cstdint>

#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "intrface.h"
#include "keytable_actions.h"

static CMapCell* CellOf(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

// 0x1014dac8 (136 bytes): a random kind 1 to count - 1 (an unsigned modulo; the count is never 1)
int32_t place_litter() {
    uint16_t id = gIntrface.mF24;   // read before GetRand
    uint32_t rand = gGameWorld->GetRand();
    uint32_t divisor = gLitterKindCount - 1;
    uint32_t kind = rand - (rand / divisor) * divisor + 1;
    CellOf(id)->PlaceLitter(static_cast<CMapCell::LitterType>(kind));
    return 1;
}

// 0x1014db74 (92 bytes): litter kind 8 (the bomb)
int32_t place_bomb() {
    CellOf(gIntrface.mF24)->PlaceLitter(static_cast<CMapCell::LitterType>(8));
    return 1;
}
