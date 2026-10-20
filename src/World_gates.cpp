// The park gates and the arrival vehicle (main app, SimTheme Park 2.04b3, World.cpp; unit w193): CWorld::SetParkOpenOrClosed (0x1013fc3c),
// ParkGates_Open (0x1014007c), ParkGates_Close (0x1014013c), ArrivalVehicle_DoNextLeg (0x10140234), ArrivalVehicle_GetStatus (0x101406cc),
// ArrivalVehicle_CrossingIsClear (0x1014080c), ArrivalVehicle_PeopleWaitingToLeave (0x10140b6c), ArrivalVehicle_PeopleLeftToDropOff
// (0x10140d98) and ArrivalVehicle_IsSeagoing (0x10140de0).
//
// The gates are the "gates" feature object (mGatesThing): its RSSE script's variable 0 is the order (0 close, 1 open, 2 close after the
// visitors have gone) and variable 1 the state. The arrival vehicle is one of three feature objects of the ride database ("bus", "seaplane",
// "Ferry"), made on demand and driven through the script's variable 0 (go) and 1 (status). Every walk or read of the thing table asserts that
// entry 0 is free first; the release GAME_ASSERTMSG does nothing, so the assert changes only the call log. Every callee outside the unit is
// untranslated or translated elsewhere (the thing virtuals, the script variables, the message centre, the ride database, the park analyser).
#include "World.h"

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "MapCell.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "platform/macdoze.h"
#include "rsse.h"

// The three vehicle names (see World.h): __sinit_World_cpp (0x10141c14) builds the strings on the Mac; until it is translated they are
// defined here with their texts.
TbDynamicStringTemplate<char16_t> gArrivalBusName(u"bus");
TbDynamicStringTemplate<char16_t> gArrivalSeaplaneName(u"seaplane");
TbDynamicStringTemplate<char16_t> gArrivalFerryName(u"Ferry");

namespace {

// The "no thing" id. The Mac reads it from nine separate 16 bit words in initialised data (0x102a46d4, 0x102a46d6, 0x102a46da, 0x102a46dc,
// 0x102a46de, 0x102a46e0, 0x102a46e2, 0x102a46e4, 0x102a46e6), all 0 and never written.
constexpr uint16_t kNoThing = 0;

// The strings at 0x10247498 + the offset in the comment.
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x151
const char kClosedText[] = "closed";                                                                   // +0x6d8
const char kOpenedText[] = "opened";                                                                   // +0x6df
const char kParkText[] = "\n*** You have just %s your park ***\n";                                     // +0x6e6
const char kNoVehicleText[] = "Fatal error: Could not find either the bus or the plane feature";      // +0x70b
const char kNoFeatureText[] = "Could not find the 'vehicle_name was here' feature\n";                  // +0x74b

// The trace level of the park message (a magic number of the original).
constexpr uint32_t kParkTrace = 0x75bcd15;

// The kind byte of a person and the word of its state at +0x220 (the names are those of src/ParkAnalyser_visitors.cpp); 0x15 is the state
// of a visitor waiting to leave by the vehicle.
constexpr uint8_t kKindPerson = 1;
constexpr uint32_t kPeepState = 0x220;
constexpr int32_t kPeepStateGettingOff = 0x15;
constexpr int32_t kCellBytes = 0x44;

// The message centre the Mac builds on first use (src/BankAccount.cpp has the same helper).
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The thing of a table row, after the assert on row 0 that every read makes. A free row answers NULL (the Mac then reads through it).
void* RowThing(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    return gThingArray[id].mThing;
}

CObject* ObjectOfRow(uint16_t id) { return static_cast<CObject*>(RowThing(id)); }

// The script of an object: the handle at +0x20 is read at each use.
RSSE_SCRIPT* ScriptOf(const CObject* object) { return RSSE_GetScriptPointer(static_cast<int32_t>(object->mF20)); }

int32_t ReadWord(const void* thing, std::size_t offset) {
    int32_t word;
    std::memcpy(&word, static_cast<const unsigned char*>(thing) + offset, sizeof word);
    return word;
}

// The map cell of a 1 based cell id: the map plus (id - 1) * 0x44 (id 0 gives the 0x44 bytes before the map).
const CMapCell* CellOf(const CMap* map, uint16_t id) {
    return reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(map) +
                                              static_cast<intptr_t>((static_cast<int32_t>(id) - 1) * kCellBytes));
}

// The body of ArrivalVehicle_GetStatus: -1 without a vehicle, else variable 1 of its script; a 6 (an error) sends the vehicle away
// (variable 1 is cleared and the world forgets the vehicle) and answers -1. The Mac repeats this code inline in CrossingIsClear.
int32_t VehicleStatus(CWorld& world) {
    if (world.mArrivalVehicleThing == kNoThing) return -1;
    const CObject* vehicle = ObjectOfRow(world.mArrivalVehicleThing);
    const int32_t status = RSSE_Getvar(ScriptOf(vehicle), 1);
    if (status != 6) return status;
    if (world.mArrivalVehicleThing != kNoThing) {
        const CObject* again = ObjectOfRow(world.mArrivalVehicleThing);
        RSSE_Setvar(ScriptOf(again), 1, 0);
        world.mArrivalVehicleThing = 0;
    }
    return -1;
}

}  // namespace

// 0x1013fc3c (780 bytes). With a gate flag only the gates are closed (ParkGates_Close(flag)). Else open == 0 closes the park: the first time
// (the word at +0x1da710 was 0) the word is set, the gates are closed when nobody is in the park and the gates answer state 1, and every
// choosable object (mFlags bit 4) of the world's object list is closed. open != 0 opens it: the word is cleared, the gates opened and every
// choosable object that IsOpenable reopened. The event (4 closed, 3 opened) is posted and the trace made even when the word did not change.
// IsParkClosed is asked after the word was set, so the Mac's test is only ever true in a real run (the tests queue it).
void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) {
    if (gateFlag != 0) {
        ParkGates_Close(gateFlag);
        return;
    }
    if (open == 0) {
        if (mF1DA710 == 0) {
            mF1DA710 = 1;
            if (IsParkClosed() != 0) {
                // GetParkAnalyser is inline here: the id is read before the assert.
                const uint16_t analyserId = mParkAnalyserThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
                const CParkAnalyser* analyser = static_cast<const CParkAnalyser*>(gThingArray[analyserId].mThing);
                if (analyser->NumberOfVisitorsInPark() == 0) {
                    if (ParkGates_GetStatus() == 1) ParkGates_Close(0);
                }
            }
            uint16_t id = mF1DA746;
            while (id != kNoThing) {
                CObject* object = ObjectOfRow(id);
                if ((object->mFlags & 4) != 0) object->CloseObject();
                id = object->mF0C;
            }
        }
        CMsgEvent message(static_cast<EventType>(4));
        MessageCentreForPost()->PostMessage(message);
    } else {
        if (mF1DA710 != 0) {
            mF1DA710 = 0;
            ParkGates_Open();
            uint16_t id = mF1DA746;
            while (id != kNoThing) {
                CObject* object = ObjectOfRow(id);
                if ((object->mFlags & 4) != 0) {
                    if (object->IsOpenable() != 0) object->Reopen();
                }
                id = object->mF0C;
            }
        }
        CMsgEvent message(static_cast<EventType>(3));
        MessageCentreForPost()->PostMessage(message);
    }
    CTRACE(kParkTrace, kParkText, mF1DA710 != 0 ? kClosedText : kOpenedText);
}

// 0x1014007c (148 bytes). Reads variable 1 of the gates object's script (the answer is dropped) and sets variable 0 to 1. Unlike
// ParkGates_GetStatus the gates thing id is not tested: with no gates object the Mac reads through the NULL thing of row 0.
void CWorld::ParkGates_Open() {
    const CObject* gates = ObjectOfRow(mGatesThing);
    RSSE_Getvar(ScriptOf(gates), 1);
    RSSE_Setvar(ScriptOf(gates), 0, 1);
}

// 0x1014013c (200 bytes). As ParkGates_Open, but variable 0 is set to 0 for flag 0 and to 2 for any other flag.
void CWorld::ParkGates_Close(int32_t flag) {
    const CObject* gates = ObjectOfRow(mGatesThing);
    RSSE_Getvar(ScriptOf(gates), 1);
    if (flag == 0) {
        RSSE_Setvar(ScriptOf(gates), 0, 0);
    } else {
        RSSE_Setvar(ScriptOf(gates), 0, 2);
    }
}

// 0x10140234 (1120 bytes). With a vehicle in use its script variable 0 is set to 1 (go). Else a vehicle is chosen and made: leg 0 picks at
// random from the world's generator (state = state * 0x19660d + 0x3c6ef35f, then the absolute value of the state modulo 3: 0 bus, 1 seaplane,
// 2 ferry), legs 1 to 3 pick 0 to 2. The wanted feature object is the one of that type's slot (mBusThing, mSeaplaneThing, mFerryThing) when
// it exists (variable 0 is set to 1), else it is made from the ride database's record of that name on map cell 1 (variable 1 is set to 1, the
// other path's variable 0 is the Mac's own quirk) and its id kept in the slot. A name the database lacks is traced and the next type is tried
// in the order ferry, seaplane, bus; when all three have failed the Mac asserts and answers no vehicle. A leg outside 0 to 3 chooses nothing
// and the Mac then reads address 0 (not reproduced: the translation answers the vehicle in use). The answer is the vehicle in use, through a
// hidden result.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::ArrivalVehicle_DoNextLeg(int32_t leg) {
    if (mArrivalVehicleThing != kNoThing) {
        const CObject* vehicle = ObjectOfRow(mArrivalVehicleThing);
        RSSE_Setvar(ScriptOf(vehicle), 0, 1);
        return {mArrivalVehicleThing};
    }
    TbDynamicStringTemplate<char16_t> name;
    gRideDatabase.GetFirstRide();   // the answer is dropped
    uint32_t tried = 0;             // bit 1 bus, 2 seaplane, 4 ferry: the types chosen so far
    uint16_t* slot = nullptr;
    for (;;) {
        if (tried == 0) {
            int32_t choice;
            if (leg == 0) {
                mStartTime = mStartTime * 0x19660du + 0x3c6ef35fu;
                uint32_t state = mStartTime;
                if (static_cast<int32_t>(state) < 0) state = 0u - state;
                choice = static_cast<int32_t>(state % 3);
            } else {
                choice = leg - 1;
            }
            if (choice == 1) {
                name = gArrivalSeaplaneName;
                tried = 2;
                slot = &mSeaplaneThing;
            } else if (choice == 0) {
                name = gArrivalBusName;
                tried = 1;
                slot = &mBusThing;
            } else if (choice == 2) {
                name = gArrivalFerryName;
                tried = 4;
                slot = &mFerryThing;
            } else {
                break;
            }
        } else if ((tried & 4) == 0) {
            name = gArrivalFerryName;
            tried += 4;
            slot = &mFerryThing;
        } else if ((tried & 2) == 0) {
            name = gArrivalSeaplaneName;
            tried += 2;
            slot = &mSeaplaneThing;
        } else {
            if ((tried & 1) != 0) {
                GAME_ASSERTMSG(0, kNoVehicleText);
                break;
            }
            name = gArrivalBusName;
            tried += 1;
            slot = &mBusThing;
        }
        if (*slot != kNoThing) {
            const CObject* existing = ObjectOfRow(*slot);
            RSSE_Setvar(ScriptOf(existing), 0, 1);
            mArrivalVehicleThing = *slot;
            break;
        }
        RideInfo* ride = gRideDatabase.GetFirstRide();
        while (ride != nullptr && NS_MacDoze::_wcsicmp(ride->mItemName, name.mData) != 0) ride = gRideDatabase.GetNextRide();
        if (ride == nullptr) {
            CTRACE(2, kNoFeatureText);
            continue;
        }
        // The Mac tests the allocation for NULL and still asks for the id (the host's allocation never fails: not reproduced, as in unit w117).
        void* block = ::operator new(0x448);
        CObject* object = new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1},
                                              tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
        *slot = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
        RSSE_Setvar(ScriptOf(object), 1, 1);
        mArrivalVehicleThing = *slot;
        break;
    }
    return {mArrivalVehicleThing};
}

// 0x101406cc (264 bytes). -1 without a vehicle; else variable 1 of its script (see VehicleStatus for the 6).
int32_t CWorld::ArrivalVehicle_GetStatus() { return VehicleStatus(*this); }

// 0x1014080c (804 bytes). 1 when there is no vehicle or it is seagoing. Else the status (VehicleStatus, inline on the Mac) is read up to three
// times, each time afresh: 3 answers 0; then more than 9 people left to drop off answers 1; then status 1 answers 0; then status 2 answers 0
// and anything else 1.
int32_t CWorld::ArrivalVehicle_CrossingIsClear() {
    if (mArrivalVehicleThing == kNoThing || ArrivalVehicle_IsSeagoing() != 0) return 1;
    if (VehicleStatus(*this) == 3) return 0;
    if (ArrivalVehicle_PeopleLeftToDropOff() > 9) return 1;
    if (VehicleStatus(*this) == 1) return 0;
    return VehicleStatus(*this) != 2 ? 1 : 0;
}

// 0x10140b6c (488 bytes). 1 when a visitor in state 0x15 stands on one of the four map cells beside the bus stop A: the stop, the cell to its
// right, and the same two cells two rows up. Each cell id is the sum cut to 16 bits (the Mac adds (dy << 7) + stop + dx and stores a halfword,
// so a stop near the edge wraps). Only the FIRST thing of each cell is looked at: the Mac reads the thing's cell link (mCellNext) and drops it.
int32_t CWorld::ArrivalVehicle_PeopleWaitingToLeave() {
    const CMap* map = GetMap();
    const int32_t dx[4] = {0, 1, 0, 1};
    const int32_t dy[4] = {0, 0, -2, -2};
    uint16_t cells[4];
    for (int32_t i = 0; i < 4; ++i) {
        const uint16_t stop = GetMap()->GetBusStopPos(0).mIndex;
        cells[i] = static_cast<uint16_t>(dx[i] + (dy[i] << 7) + stop);
    }
    for (int32_t i = 0; i < 4; ++i) {
        const uint16_t id = CellOf(map, cells[i])->mWho;
        if (id != kNoThing) {
            const CThing* thing = static_cast<const CThing*>(RowThing(id));
            if (thing->mKind == kKindPerson && ReadWord(thing, kPeepState) == kPeepStateGettingOff) return 1;
        }
    }
    return 0;
}

// 0x10140d98 (8 bytes). The people still on the bus, the word at +0x2d0 (CArrival's mPeopleOnBus).
int32_t CWorld::ArrivalVehicle_PeopleLeftToDropOff() { return mArrival.mPeopleOnBus; }

// 0x10140de0 (76 bytes). 1 when a vehicle is in use and it is not the bus (the seaplane and the ferry sail).
int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    if (mArrivalVehicleThing != kNoThing && mBusThing != mArrivalVehicleThing) return 1;
    return 0;
}
