// keytable actions at 0x1014f16c to 0x1014fb2c: win_ticket_7/8/9, enter_hoarding_mode, force_ride_breakdown, park_attraction_value.
#include <new>

#include "keytable_actions.h"
#include "analyser.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "intrface.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"

namespace {

// The shared body of win_ticket_7 to 9: create the master save on first use, then earn the ticket.
int32_t WinTicket(int32_t type) {
    if (gMasterSave == nullptr) {
        void* mem = ::operator new(0x4c);
        CFSGlobalMasterSave* save = nullptr;
        if (mem != nullptr) save = new (mem) CFSGlobalMasterSave();
        gMasterSave = save;
    }
    gMasterSave->EarnedTicket(static_cast<GoldenTicketTypeGlobal>(type), 100);
    return 1;
}

}  // namespace

int32_t win_ticket_7() { return WinTicket(1); }   // 0x1014f16c
int32_t win_ticket_8() { return WinTicket(2); }   // 0x1014f208
int32_t win_ticket_9() { return WinTicket(3); }   // 0x1014f2a4

int32_t enter_hoarding_mode() {   // 0x1014f494
    CMMPathDraw* path = nullptr;
    void* pathMem = ::operator new(0x14);
    if (pathMem != nullptr) path = new (pathMem) CMMPathDraw(static_cast<CELL_MODES>(0x39));
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(::operator new(1));
        if (mouse != nullptr) {
            CMMDefault* def = nullptr;
            void* defMem = ::operator new(8);
            if (defMem != nullptr) def = new (defMem) CMMDefault();
            mouse->AssignMouseMode(def);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(path);
    return 1;
}

int32_t force_ride_breakdown() {   // 0x1014f7e0
    CThing* under = gIntrface.mThingUnderPointer;
    if (under == nullptr) return 0;
    // The listing asks GetThingId of the pointed thing's slot (r4 is the thing, r3 the result slot).
    auto id = under->GetThingId();
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    CObject* object = static_cast<CObject*>(gThingArray[id.mIndex].mThing);
    if (object != nullptr) {
        if (!object->IsBrokenDown()) object->RequestBreakdown();
    }
    return 1;
}

int32_t park_attraction_value() {   // 0x1014fa2c
    gGameWorldRef->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    gGameWorldRef->mParkTime.GetFunnyTimeSinceGameTurn(0);
    int32_t points = gGameWorld->GetParkAnalyser()->GetTotalParkAttractionPoints();
    CTRACE(3, "Park attraction points: %d\n", points);
    uint8_t first, second, third;
    gGameWorldRef->GetParkAnalyser()->GetThreeMostDominantThoughts(first, second, third);
    CTRACE(3, "Most dominant thoughts: %2d, %2d, %2d\n", first, second, third);
    return 1;
}
