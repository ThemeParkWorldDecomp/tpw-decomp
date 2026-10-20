// intrface.cpp range of the main app (SimTheme Park 2.04b3), unit w133: CIntrface::DoPointedThingComputations (0x1007dbb8, 500 bytes).
#include <cstdint>

#include "Debug.h"
#include "LLCellSystem.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"

// Works out what the pointer is over. Both the staff-or-guest slot and the under-pointer slot are cleared. When there is a pointed thing
// and its kind is one of 1, 4, 5, 6, 7, 8 or 0x12 (a person, a staff member, a guest, a mechanic and so on), it goes in the staff-or-guest
// slot. The cell column of the id the pointer is over (mF24 - 1, low 7 bits) is looked up with LLCR_OverRide; unless it answers the "no
// cell" halfword, the thing array's row 0 must be free (GAME_ASSERTMSG) and the thing in the answered row goes in the under-pointer slot.
// Last, a pointed thing that is not set becomes the thing under the pointer.
void CIntrface::DoPointedThingComputations() {
    mPointedStaffOrGuest = nullptr;
    mThingUnderPointer = nullptr;
    if (mPointedThing != nullptr) {
        const uint8_t kind = mPointedThing->mKind;
        if (kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12) mPointedStaffOrGuest = mPointedThing;
    }
    uint16_t answer;
    const uint32_t cell = static_cast<uint32_t>(mF24) - 1;
    LLCR_OverRide(&answer, static_cast<int32_t>(cell & 0x7f), static_cast<int32_t>(cell >> 7));
    if (answer != gLLCRNoCell) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        mThingUnderPointer = static_cast<CThing*>(gThingArray[answer].mThing);
    }
    if (mPointedThing == nullptr) mPointedThing = mThingUnderPointer;
}
