// CGame::InitWorld (main app of SimTheme Park 2.04b3; unit w113).
#include <new>

#include "Game.h"
#include "World.h"

// 0x10147e34 (136 bytes): creates the world. `this` (the CGame) is never read. The world pointer is stored into gGameWorld BEFORE the
// NULL test, so an allocation that fails overwrites a previous world pointer with NULL (the answer is then 0 and nothing else runs).
// gGameWorld is not cleared when Allocate fails (it cannot: AllocateThingHeap always answers 1).
int32_t CGame::InitWorld() {
    CWorld* const world = new (std::nothrow) CWorld;
    gGameWorld = world;
    if (world == nullptr) return 0;
    if (!gGameWorld->Allocate()) return 0;
    return 1;
}
