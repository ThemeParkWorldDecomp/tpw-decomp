// CWorld::ExitLevel, CWorld::Free and CWorld::FreeThingHeap (main app, SimTheme Park 2.04b3, World.cpp; unit w81).
#include "World.h"

#include "Mouse.h"

// 0x1013a9fc (184 bytes). Flag word 0x1da738 is 2 while the level is taken apart and 1 afterwards. The mouse, when there is one, is
// told to drop its mode and deleted (`delete` of a CMouse is that call and then the plain operator delete), the global cleared after.
// The map is the one of the WORLD gGameWorld points to (the Mac reads that global again here), not of `this`. DeleteAllThings answers
// something in r3 which the Mac hands on to Vehicle::clear_behaviours, but that function takes no argument and overwrites r3 first.
void CWorld::ExitLevel() {
    mF1DA738 = 2;
    CMouse* mouse = gMouse;
    if (mouse != nullptr) {
        delete mouse;
    }
    gMouse = nullptr;
    gGameWorld->GetMap()->DeleteMapInEngine();
    DeleteAllThings();
    Vehicle::clear_behaviours();
    BMP_CloseSystem();
    mF1DA738 = 1;
}

// 0x1013a264 (36 bytes).
void CWorld::Free() { FreeThingHeap(); }

// 0x1013ac08 (24 bytes). The Mac counts a register up to 0x2800 and does nothing in the loop (the compiler kept the loop, its body
// compiled to nothing). Kept as a loop for the record; the host compiler drops it, which nothing can observe.
void CWorld::FreeThingHeap() {
    for (uint32_t slot = 0; slot < 0x2800; ++slot) {
    }
}
