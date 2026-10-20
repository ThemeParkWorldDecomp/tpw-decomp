// CWorld::Allocate (World.cpp, main app of SimTheme Park 2.04b3; unit w113), in a file of its own so that a test library can fake
// AllocateThingHeap and reach the error branch.
#include "BlockSave.h"
#include "ExitErr.h"
#include "World.h"

// 0x1013a1c0 (124 bytes). AllocateThingHeap always answers 1, so the error branch (CExitError type 8, value 0, no text) is dead code.
// The save block name is the original's own typo ("World Struture"); RegisterSaveBlock drops it.
bool CWorld::Allocate() {
    if (!AllocateThingHeap()) {
        gExitError.SetError(CExitError::kType8, 0, nullptr);
        return false;
    }
    gBlockSave.RegisterSaveBlock(CBlockSave::kWorld, this, sizeof(CWorld), "World Struture");
    return true;
}
