// CObject::CObject, main app 0x10105d74 (1,800 bytes), and CObject::SetState, 0x1010dc94 (2,000 bytes), unit w126.
// The Mac files them under Object.cpp. The constructor builds a ride, shop or other object of the park from its ride record; SetState
// moves it to one of the five states and, for the three that interrupt it (broken down, wants an upgrade, condemned), sends the rider home
// and posts the matching message.
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "rsse.h"

// The two CPerson members SetState calls; CPerson is a placeholder class here (ThingReceivers.h), so they are declared against their Mac names.
void ObjectForceRideAdmittance(CPerson*) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");   // 0x1011df70
void ObjectDismissFromQueue(CPerson*) __asm__("__ZN7CPerson16DismissFromQueueEv");         // 0x1011f1b8

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

// The lazily made message centre: allocated (0x1d0 bytes, 29 subscription headers) when the global is still NULL, a failed allocation leaves it
// NULL. The Mac re-reads the global before every use.
CMessageCentre* Centre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The words of the ride record, the object controls and a person that the host's own layouts do not reach by a name: read at their Mac offsets.
uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof value);
    return value;
}
void SetWordAt(void* base, std::size_t offset, uint32_t value) {
    std::memcpy(static_cast<unsigned char*>(base) + offset, &value, sizeof value);
}
// The ride record's value slot `slot` as the Mac lays it out: 0xc + 4 * slot (the host's own record has 8 byte slots).
int32_t RideValue(const RideInfo* ride, RideBalanceSlot slot) { return ride->Value(slot).mInteger; }

// The cell the effector is added to: the object's own cell as a 1 based map id.
uint16_t OwnCell(const CObject* object) {
    return static_cast<uint16_t>(object->mBase.mCellX + object->mBase.mCellY * 0x80 + 1);
}

void CopyStamp(uint32_t (&target)[2], const TbTimeStamp& stamp) {
    uint32_t words[2];
    std::memcpy(words, &stamp, sizeof words);
    target[1] = words[1];
    target[0] = words[0];
}

CGoldenTicketControl* GoldenTickets() {
    return reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<uintptr_t>(gWorld) + 0x2c0);
}
CControlManager* Controls() {
    return reinterpret_cast<CControlManager*>(reinterpret_cast<uintptr_t>(gWorld) + 0x1d943c);
}

// The three interrupting states of SetState share this: whoever the script says is on the ride (or still queued) is sent away, the ride
// stops, the loaded person is forgotten, script variable 6 is set and the hoardings are lit. Every call to the assertion and the traces is in
// the Mac's order (the code is the same three times in the original).
void StopRide(CObject* self) {
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(self->mF20));
    const uint32_t variable = static_cast<uint32_t>(RSSE_Getvar(script, 0));
    const uint16_t queued = self->mQueuedPerson;
    bool dismiss = true;
    CPerson* person = nullptr;
    if (variable != queued) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        person = static_cast<CPerson*>(gThingArray[queued].mThing);
        if (WordAt(person, 0x220) == 0xe) {
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> personId = reinterpret_cast<CThing*>(person)->GetThingId();
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> rideId = self->mBase.GetThingId();
            CTRACE(2, "Object %d: script admitted person %d but he doesn't know yet, forcing him onto ride\n", rideId.mIndex, personId.mIndex);
            ObjectForceRideAdmittance(person);
            dismiss = false;
        }
    }
    if (dismiss && self->mQueuedPerson != 0) {
        const uint16_t again = self->mQueuedPerson;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        person = static_cast<CPerson*>(gThingArray[again].mThing);
        self->RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{self->mQueuedPerson});
        ObjectDismissFromQueue(person);
    }
    CTRACE(2, "Object %d: Closing...", self->mBase.GetThingId().mIndex);
    self->mF64 = 0;
    const uint16_t loaded = self->mPersonLoaded;
    CTRACE(2, "Object %d: person being loaded was %d (about to zeroise)\n", self->mBase.GetThingId().mIndex, loaded);
    self->mPersonLoaded = 0;
    RSSE_SCRIPT* second = RSSE_GetScriptPointer(static_cast<int32_t>(self->mF20));
    RSSE_Setvar(second, 6, 1);
    ActivateHoardings(gRideAnimSets[static_cast<int32_t>(self->mF1C)], 1);
}
}  // namespace

// The state is only stored for 0 and 3; 2 (wants an upgrade), 1 (broken down) and 4 (condemned, which also traces "Ride has become
// CONDEMNED!!!" at level 0xc first) stop the ride and post their message; any other value asserts. The state is stored last in every case.
void CObject::SetState(ObjectState state) {
    if (state == kStateWantsUpgrade) {
        StopRide(this);
        CMsgRideWantsUpgrade message(mBase.GetThingId());
        Centre()->PostMessage(message);
    } else if (state < 2) {
        if (state != 0) {
            if (state < 0) {
                GAME_ASSERTMSG(0, "Unknown state in CObject::SetState\n");
            } else {
                StopRide(this);
                CMsgRideBrokenDown message(mBase.GetThingId());
                Centre()->PostMessage(message);
            }
        }
    } else if (state != 4) {
        if (state >= 4) GAME_ASSERTMSG(0, "Unknown state in CObject::SetState\n");
    } else {
        CTRACE(0xc, "Ride has become CONDEMNED!!!\n");
        StopRide(this);
        CMsgRideCondemned message(mBase.GetThingId());
        Centre()->PostMessage(message);
    }
    mState = state;
}

// The order of the stores and calls is the listing's. The ride record (GetDBInfo of the object type) decides everything after the first
// block of zeroing: the three rotated map offsets (the object's own cell, the entry and the exit), the starting values, the flags (the
// regional effectors for relief, security and fireworks), the golden ticket or the cost of the ride, the first state (0 for a choosable ride,
// else 3), the operating speed, capacity and duration, the build time, the object type's control and the sign texts. The ride's name is copied
// into a local buffer that nothing reads (kept: the call is the original's).
CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a, int32_t b)
    : mBase(map, static_cast<CThing::ThingModel>(3)) {
    mObjectType = type.mIndex;
    mRotation = a;
    mBuilt[1] = 0;
    mBuilt[0] = 0;
    mF1C = 0;
    mF20 = 0;
    mF24 = b;
    mF28 = 1;
    mF2C = 0;
    mFlags = 0;
    mF30 = 0;
    mF32 = 0;
    mF34 = 0;
    mF36 = 0;
    mQueuedPerson = 0;
    mF3C = 0;
    mF40 = 0.0f;
    mF44 = 0.0f;
    mF48 = 0.0f;
    mF4C = 0;
    mF50 = 0;
    mF54 = 0;
    mF58 = 0;
    mF59 = 0;
    mF5A = 0;
    mF5C = 0;
    mF60 = 0;
    mF64 = 0;
    mPersonLoaded = 0;
    // The five history constructors (members) run here on the Mac, with the five stores below between the second and the third.
    mF17C = 0;
    mF180 = 0;
    mState = 0;
    mF19C = 0;
    mF1A0 = 0;
    mF0C = 0;
    gWorld->Register(this);
    Centre()->Subscribe(mBase, static_cast<MsgType>(0xb));
    {
        CMsgNewRide message(mBase.GetThingId());
        Centre()->PostMessage(message);
    }
    RideInfo* ride = GetDBInfo();
    const MapDelta offset = reinterpret_cast<const MapDelta*>(&ride->mMapOffsetX)->Rotate(a + 0xb4);
    mF30 = static_cast<uint16_t>(static_cast<uint32_t>(offset.mX) + (static_cast<uint32_t>(offset.mY) << 7) + map.mIndex);
    const MapDelta entry = reinterpret_cast<const MapDelta*>(&ride->mEntryX)->Rotate(a);
    mF32 = static_cast<uint16_t>(static_cast<uint32_t>(entry.mX) + (static_cast<uint32_t>(entry.mY) << 7) + mF30);
    const MapDelta exit = reinterpret_cast<const MapDelta*>(&ride->mExitX)->Rotate(a);
    mF34 = static_cast<uint16_t>(static_cast<uint32_t>(exit.mX) + (static_cast<uint32_t>(exit.mY) << 7) + mF30);
    mF40 = 100.0f;
    mF48 = 100.0f;
    mF44 = 100.0f;
    mF60 = 0;
    mF4C = 0;
    mF50 = 0;
    mCostOfGoods = static_cast<uint32_t>(RideValue(ride, kRideSlot_InitCostOfGoods));
    mF188 = 0x32;
    mChanceOfNotLoosing = 100 - RideValue(ride, kRideSlot_InitChanceOfLoosing);
    mPricePerUse = static_cast<uint32_t>(RideValue(ride, kRideSlot_InitPricePerUse));
    mF194 = 0x32;
    mF64 = 1;
    mPersonLoaded = 0;
    if (RideValue(ride, kRideSlot_ProvidesRelief) != 0) {
        mFlags |= 1;
        gWorldMap->AddRegionalEffector(kRegionEffectorRelief, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{OwnCell(this)});
    }
    if (RideValue(ride, kRideSlot_HoldsLitter) != 0) mFlags |= 0x40;
    if (RideValue(ride, kRideSlot_ChillsYouOut) != 0) mFlags |= 2;
    if (RideValue(ride, kRideSlot_IsChoosable) != 0) mFlags |= 4;
    if (RideValue(ride, kRideSlot_RideHandlesSprite) != 0) mFlags |= 0x20;
    if (RideValue(ride, kRideSlot_HasQueue) != 0) mFlags |= 8;
    if (RideValue(ride, kRideSlot_ProvidesSecurity) != 0) {
        mFlags |= 0x10;
        gWorldMap->AddRegionalEffector(kRegionEffectorSecurity, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{OwnCell(this)});
    }
    if (RideValue(ride, kRideSlot_IsFireworks) != 0) {
        mFlags |= 0x80;
        gWorldMap->AddRegionalEffector(kRegionEffectorFireworks, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{OwnCell(this)});
    }
    if (RideValue(ride, kRideSlot_RunsContinuously) != 0) mFlags |= 0x100;
    CTRACE(2, "Can only build objects at upgrade level 0 at the moment\n");
    const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideType{ride->mId};
    if (RideValue(ride, kRideSlot_GoldenTicketCost) > 0 && !GoldenTickets()->HasBeenBought(rideType)) {
        GoldenTickets()->SpendTicketOn(rideType);
    } else {
        gWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(RideValue(ride, kRideSlot_Upgrades_CostOfUpgrade)));
    }
    if ((mFlags & 4) == 0) {
        SetState(kStateThree);
    } else {
        SetState(kStateNormal);
    }
    UpdateEngine(0);
    CreateRideScript();
    if (RideValue(ride, kRideSlot_Upgrades_InitSpeed) > 0) SetOperatingSpeed(static_cast<uint32_t>(RideValue(ride, kRideSlot_Upgrades_InitSpeed)));
    if (RideValue(ride, kRideSlot_Upgrades_InitCapacity) > 0) {
        SetOperatingCapacity(static_cast<uint8_t>(RideValue(ride, kRideSlot_Upgrades_InitCapacity)));
    }
    if (RideValue(ride, kRideSlot_Upgrades_InitDuration) > 0) {
        SetOperatingDuration(static_cast<uint8_t>(RideValue(ride, kRideSlot_Upgrades_InitDuration)));
    }
    CopyStamp(mBuilt, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    ObjectControl* control = Controls()->GetObjectControl(type, 0);
    if (WordAt(control, 0x1c) == 0) {
        const uint32_t turn = gWorld->mTurn;
        control = Controls()->GetObjectControl(type, 0);
        SetWordAt(control, 0x1c, turn);
    }
    control = Controls()->GetObjectControl(type, 0);
    SetWordAt(control, 0x18, WordAt(control, 0x18) + 1);
    char name[0x21];
    std::strncpy(name, ride->Value(kRideSlot_Name).mString, 0x20);
    name[0x20] = 0;
    asm volatile("" : : "r"(name) : "memory");
    gRideDatabase.GetSignNameA(type.mIndex, mSignA, 0x20);
    gRideDatabase.GetSignNameB(type.mIndex, mSignB, 0x20);
    if ((mFlags & 8) != 0) CloseObject();
    gWorld->GetParkAnalyser()->BuiltSomething();
}
