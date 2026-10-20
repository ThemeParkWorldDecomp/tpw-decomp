// CMacroAI (main app, unit w129): the world's own macro layer. The world is the CMacroAI (the staff database at +0, the park time at +0x2a0, the golden
// ticket control at +0x2c0 and the arrivals at +0x2c4 are its first 0x2d8 bytes), so `this` is viewed as the world.
#include "ObjectParts.h"
#include "World.h"

// 0x1010096c (76 bytes): the arrivals, the staff database and the park clock are set up, in that order.
void CMacroAI::Init() {
    CWorld* world = reinterpret_cast<CWorld*>(this);
    world->mArrival.Init();
    world->mStaffDB.Init();
    world->mParkTime.Init();
}

// 0x101009dc (136 bytes): the arrivals, the staff database and the park clock are updated, in that order. Then, when the turn is a multiple of 100 (an
// UNSIGNED remainder), the golden tickets are checked. The turn is read from *gGameWorldRef (the pointer at 0x102a460c names gGameWorld), not from `this`
// and not from gWorld, which the staff database and the park clock read through CWorldSavvy::GetWorld.
void CMacroAI::Update() {
    CWorld* world = reinterpret_cast<CWorld*>(this);
    world->mArrival.Update();
    world->mStaffDB.Update();
    world->mParkTime.Update();
    const uint32_t turn = gGameWorldRef->mTurn;
    if (turn % 100 == 0) world->mParkTime.GoldenTickets()->CheckForTickets();
}
