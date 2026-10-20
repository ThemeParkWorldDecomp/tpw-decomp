// CWorld's construction (World.cpp, main app of SimTheme Park 2.04b3; unit w113): the constructor, AllocateThingHeap, InitThingHeap
// and ObjectControl's empty constructor. Allocate is in World_alloc.cpp.
#include "World.h"

#include "CellBase.h"
#include "ObjectControl.h"

// 0x1013a10c (4 bytes): a bare blr. The 0x96 object controls of the world are never initialised.
ObjectControl::ObjectControl() {}

// 0x10139f70 (376 bytes). The Mac builds the members in this order: the staff database, the park time, the arrival settings, the map
// cells (0x4000 of 0x44 bytes), the track cells (0x4000 of 0x28), the region effect cells (0x4000 of 10), then the two grid
// registrations, then the object controls (0x96 of 0x20), and last its own stores. The host's implicit member construction runs in
// declaration order (the object controls before the registrations): nothing can tell, ObjectControl's constructor is empty and the
// registrations touch only their globals. No other member is initialised: a world built over 0xa5 keeps 0xa5 there.
CWorld::CWorld() {
    CCellBase::RegisterMapGrid(mMapCells, sizeof(CMapCell));
    CCellBase::RegisterTrackGrid(mTrackCells, sizeof(CTrackCell));
    // Each halfword goes through a stack halfword on the Mac; the value is 0.
    mF1DA73C = 0;
    mF1DA73E = 0;
    mF1DA740 = 0;
    mF1DA744 = 0;
    mF1DA742 = 0;
    mF1DA746 = 0;
    mF1DA710 = 1;
    gFreeThingHead = &gThingArray[0];
    gUsedThingHead = nullptr;
}

// 0x1013aadc (60 bytes): the world pointer is stored first, then the map pointer (the Mac adds 0x2d8 with no NULL test), then the
// thing heap is built.
bool CWorld::AllocateThingHeap() {
    gWorld = this;
    gWorldMap = GetMap();
    InitThingHeap();
    return true;
}

// 0x1013ab48 (148 bytes), a leaf that never reads `this`. Entry 0 is the "no thing" sentinel (index 0, never on the free list);
// entries 1 to 0x27fe chain to their successor; entry 0x27ff ends the free chain. Every word of every entry is written; the loop
// counter is unsigned (cmplwi). The used list starts empty and the free list at entry 1.
void CWorld::InitThingHeap() {
    CThingArrayEntry* const table = gThingArray;
    table[0].mNextFree = nullptr;
    table[0].mNextUsed = nullptr;
    table[0].mPrevUsed = nullptr;
    table[0].mThing = nullptr;
    table[0].mF04[0] = 0;
    for (uint32_t i = 1; i < 0x27ff; ++i) {
        table[i].mNextFree = &table[i + 1];
        table[i].mNextUsed = nullptr;
        table[i].mPrevUsed = nullptr;
        table[i].mThing = nullptr;
        table[i].mF04[0] = i;
    }
    table[0x27ff].mNextFree = nullptr;
    table[0x27ff].mNextUsed = nullptr;
    table[0x27ff].mPrevUsed = nullptr;
    table[0x27ff].mThing = nullptr;
    table[0x27ff].mF04[0] = 0x27ff;
    gUsedThingHead = nullptr;
    gFreeThingHead = &table[1];
}
