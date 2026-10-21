// Object.cpp: name proven by __sinit_Object_cpp in the main application.

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "History.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "ObjectSerialise.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

// CObject::CObject, main app 0x10105d74 (1,800 bytes), and CObject::SetState, 0x1010dc94 (2,000 bytes), unit w126.
// The Mac files them under Object.cpp. The constructor builds a ride, shop or other object of the park from its ride record; SetState
// moves it to one of the five states and, for the three that interrupt it (broken down, wants an upgrade, condemned), sends the rider home
// and posts the matching message.

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

// The ride messages (main app, unit w200): the constructors 0x10151998, 0x10151a14, 0x10151a98, 0x10151b1c and 0x10151e48, the GetType functions
// 0x10110f6c, 0x10110e84, 0x10110ec0, 0x10110e44, 0x10110f34 and 0x10110efc, and the deleting destructors 0x101064fc, 0x1010e524, 0x1010e5a0, 0x1010e4a4,
// 0x10107b28 and 0x1010aa34. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgNewRide::~CMsgNewRide() {}

// Object.cpp saved ride state (unit w229).

// __sinit_Object_cpp, 0x10111e94 (88 bytes): ordered definitions replace its stores,
// string construction and runtime destructor registration (Mac node 0x1034c374).
int32_t gObjectStaticC384 = 0; // 0x1034c384, meaning unknown
int32_t gObjectStaticC380 = 0x10000; // 0x1034c380, meaning unknown
TbDynamicStringTemplate<char16_t> gObjectStaticC360(u"gms.dat"); // 0x1034c360

// 0x10111958, 236 bytes. Failure leaves the file's raw bytes in the destination.
template<> int32_t LOADVAR<CObject::ObjectState>(void* file, CObject::ObjectState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x10111bbc, 260 bytes. The swapped copy never changes the source.
template<> int32_t SAVEVAR<CObject::ObjectState>(void* file, CObject::ObjectState* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

// Declared before its first use: the definition is further down.
template<> int32_t LOADVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t SAVEVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog);

// 0x10106570, 4,368 bytes. Every failed call keeps its mutations and stops here.
int32_t CObject::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= 2) return 0;
    using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
    using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
    using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

    // Labels are the strings at 0x1024294c + 0x39 through +0x242.
#define OBJECT_VALUE(Type, member, name) \
    if ((save != 0 ? SAVEVAR<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0) \
                   : LOADVAR<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0)) != 0) return 1
#define OBJECT_STRUCT(Type, member, name) \
    if ((save != 0 ? SAVEVARSTRUCT<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0) \
                   : LOADVARSTRUCT<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0)) != 0) return 1
#define OBJECT_HISTORY(member) \
    if (member.Serialise(file, save, version, unused) != 0) return 1

    OBJECT_VALUE(MacLong, mRotation, "mAngle");
    OBJECT_STRUCT(ObjectId, mObjectType, "mId");
    int32_t calendar[8];
    auto* stamp = reinterpret_cast<TbTimeStamp*>(mBuilt);
    if (save != 0) {
        stamp->GetTime(&calendar[0], &calendar[1], &calendar[2], &calendar[3],
                       &calendar[4], &calendar[5], &calendar[6], &calendar[7]);
    }
    for (int32_t t = 0; t < 8; ++t) {
        OBJECT_VALUE(int32_t, calendar[t], "tv[t]");
    }
    if (save == 0) {
        stamp->SetTime(calendar[0], calendar[1], calendar[2], calendar[4], calendar[5], calendar[6], calendar[7]);
    }
    OBJECT_VALUE(MacULong, mF1C, "MeshInstanceID");
    OBJECT_VALUE(uint16_t, mFlags, "mFlags");
    for (int32_t i = 0; i < 33; ++i) {
        OBJECT_VALUE(char16_t, mSignA[i], "mNameA[i]");
        OBJECT_VALUE(char16_t, mSignB[i], "mNameB[i]");
    }
    OBJECT_VALUE(MacLong, mF20, "mRideScriptHandle");
    OBJECT_VALUE(MacLong, mF24, "mTrackRideHandle");
    OBJECT_VALUE(ObjectState, mState, "mState");
    OBJECT_STRUCT(MapId, mF30, "mTopLeft");
    OBJECT_STRUCT(MapId, mF32, "mEntryPos");
    OBJECT_STRUCT(ThingId, mF0C, "mNext");
    OBJECT_STRUCT(ThingId, mF5A, "mAssignedStaffMember");
    OBJECT_STRUCT(MapId, mF36, "mBackOfQueue");
    OBJECT_VALUE(int32_t, mF64, "mCanLoad");
    OBJECT_STRUCT(MapId, mF34, "mExitPos");
    OBJECT_STRUCT(ThingId, mQueuedPerson, "mFirstInQ");
    OBJECT_VALUE(int32_t, mF28, "mIsTrackRideValid");
    OBJECT_STRUCT(ThingId, mF2C, "mUpgradeParent");
    OBJECT_HISTORY(mHistoryF4);
    OBJECT_HISTORY(mHistory6C);
    OBJECT_VALUE(uint32_t, mF19C, "mNumCustomers");
    OBJECT_HISTORY(mHistory1A4);
    OBJECT_VALUE(uint32_t, mF1A0, "mNumWalkAways");
    OBJECT_HISTORY(mHistory22C);
    OBJECT_HISTORY(mHistory2B4);
    OBJECT_HISTORY(mHistory33C);
    OBJECT_VALUE(uint8_t, mF59, "mOperatingCapacity");
    OBJECT_VALUE(uint8_t, mF58, "mOperatingDuration");
    OBJECT_VALUE(uint32_t, mF54, "mOperatingSpeed");
    OBJECT_STRUCT(ThingId, mPersonLoaded, "mPersonBeingLoaded");
    OBJECT_VALUE(MacLong, mCostOfGoods, "mCostOfGoods");
    OBJECT_VALUE(MacLong, mF188, "mQualityOfGoods");
    OBJECT_VALUE(MacLong, mChanceOfNotLoosing, "mChanceOfWinning");
    OBJECT_VALUE(MacLong, mPricePerUse, "mPricePerUse");
    if (save == 0) {
        const int32_t price = static_cast<int32_t>(mPricePerUse);
        mPricePerUse = price < 0 ? 0 : price > 500 ? 500 : price;
    }
    OBJECT_VALUE(MacLong, mF194, "mAmountOfSpecialIngredient");
    OBJECT_VALUE(uint32_t, mF3C, "mQueueSizeInCells");
    float pv;
    if (save != 0) {
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF48)));
        OBJECT_VALUE(float, pv, "pv");
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF44)));
        OBJECT_VALUE(float, pv, "pv");
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF40)));
        OBJECT_VALUE(float, pv, "pv");
    } else {
        OBJECT_VALUE(float, pv, "pv");
        mF48 = pv;
        OBJECT_VALUE(float, pv, "pv");
        mF44 = pv;
        OBJECT_VALUE(float, pv, "pv");
        mF40 = pv;
    }
    OBJECT_VALUE(int32_t, mF60, "mRequestedService");
    OBJECT_VALUE(MacULong, mF5C, "mTimeMarkedForMaintenance");
    OBJECT_VALUE(uint32_t, mF180, "mTotalCosts");
    OBJECT_VALUE(uint32_t, mF17C, "mTotalTakings");
    OBJECT_VALUE(MacLong, mF50, "mUpgradeBalloonSprite");
    OBJECT_VALUE(uint8_t, mF4C, "mUpgradeLevel");
#undef OBJECT_HISTORY
#undef OBJECT_STRUCT
#undef OBJECT_VALUE
    return 0;
}

// CObject, main app, unit w132 (the Mac files them under Object.cpp): the ride object's per-turn work UpdateTurn (0x1010d634, 684 bytes) and
// ModelState (0x1010daa0, 456 bytes), the ride script's creation CreateRideScript (0x101076b0, 328 bytes) and the engine ride's creation or
// restoring UpdateEngine (0x10107d38, 492 bytes). The ride's wear, breakdown, queue and state handlers, the script and ride instance
// makers are untranslated (the headers declare them; the launcher answers zero).

namespace {
uint32_t WordAt_2(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint32_t LowByte(float value) {
    return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xff;
}

constexpr uint16_t kNoThing = 0;   // the halfword at 0x102a0be2 and 0x102a0be4, both 0
}  // namespace

// Looks for the script "<ride folder>:<ride name>.rse" (a file name of the ride record); when it is not found says so (level 0xc) and takes the
// placeholder "Data:TestScript:test.rse". Starts the script with the ride's thing id (in the HIGH halfword of the word) and with 1 when the
// action recorder is recording, keeps its handle and tells the ride instance. The Mac's name is a wide string, widened to wchar_t for %ls here.
void CObject::CreateRideScript() {
    const char16_t* fileName = GetFileName();
    RideInfo* info = GetDBInfo();
    std::wstring wide;
    if (fileName != nullptr) {
        for (const char16_t* c = fileName; *c != 0; ++c) wide.push_back(static_cast<wchar_t>(*c));
    }
    char path[0x404];
    std::snprintf(path, sizeof(path), "%s:%ls%s", info->mFolder, fileName != nullptr ? wide.c_str() : nullptr, ".rse");
    const char* found = FindFilePath(path, 1, nullptr);
    if (found == nullptr) {
        CTRACE(0xc, "Ride script not located for %s - using placeholder\n", GetDBInfo()->mFolder);
        found = FindFilePath("Data:TestScript:test.rse", 1, nullptr);
    }
    const uint32_t thing = static_cast<uint32_t>(mBase.GetThingId().mIndex) << 16;
    if (gActionRecorder.mRecording != 0) {
        mF20 = static_cast<uint32_t>(RSSE_InitScript(found, thing, 1));
    } else {
        mF20 = static_cast<uint32_t>(RSSE_InitScript(found, thing, 0));
    }
    SetRideScriptID(mF1C, mF20);
}

// CObject, main app, unit w210 (its test; translated in unit v5L21; the Mac files them under Object.cpp, in pieces): a
// ride's names, kind, excitement and stand point, and its final destroy. UpgradesWhat (0x1011050c, 72 bytes), GetControl (0x10110580, 72), GetFileName
// (0x1011062c, 56), the sign names GetUIName (0x10110690, 264), GetSignNameA (0x101107c4, 232), GetSignNameB (0x101108dc, 60), SetSignNameA (0x10110948, 412)
// and SetSignNameB (0x10110b10, 412), the track ride test IsTrackRide (0x10110d50, 200; IsCoaster is in src/gameplay_leaves.cpp, unit v5L07), the excitement level
// (GetProspectiveExcitementLevel 0x1010cba0, 976; GetExcitementLevel 0x1010cfb0, 252), the stand point (GetStandPoint 0x1010a4fc, 1,028) and the final destroy
// (FinalDestroy 0x10107828, 724). src/Object.cpp, src/Object_turn.cpp, src/Object_queries.cpp, src/Object_queue.cpp, src/Object_takings.cpp,
// src/Object_upkeep.cpp, src/Object_opening.cpp and src/Object_admit.cpp hold other CObject members. The companions are CMsgRideDeleted's constructor
// (src/ThingMessages2.cpp) and CRideDatabase::GetFileName (src/RideDB_names.cpp).

// The two scratch buffers of the sign names (0x21 wide characters each in the original's data; the names are mine).
char16_t gObjectSignNameBuffer[0x21];   // 0x1034c2d8: GetSignNameA's
char16_t gObjectUINameBuffer[0x21];     // 0x1034c31a: GetUIName's

namespace {

// The ride record of the object's type; every use fetches it again, as the original does.
RideInfo* RecordOfType(const CObject* object) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
}

// A slot of the record moved up by the ride's upgrade level (16 slots a level).
int32_t LevelSlot(const RideInfo* info, RideBalanceSlot slot, uint8_t level) {
    return info->Value(static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16)).mInteger;
}

// The ride's own cell as a 1 based map id (the effectors and the action recorder's value).
uint16_t InfoCell(const CObject* object) {
    return static_cast<uint16_t>(object->mBase.mCellX + object->mBase.mCellY * 0x80 + 1);
}

// A speed or duration ratio held to [0.75, 1.25] (fcmpo with bge and ble: a NaN stays NaN).
float ClampRatio(float ratio) {
    if (ratio < 0.75) return 0.75f;
    if (ratio > 1.25) return 1.25f;
    return ratio;
}

// One stand point coordinate is inside (0, 1); the second compare reads a second fetch, made only when the first passes
// (a NaN fails the first).
bool StandSlotInside(const CObject* object, RideBalanceSlot slot) {
    if (!(RecordOfType(object)->Value(slot).mFloat < 1.0)) return false;
    return RecordOfType(object)->Value(slot).mFloat > 0.0;
}

}  // namespace

// The ride leaves the park: one fewer of its type, unregistered, the staff told (CMsgRideDeleted), its residual value
// refunded, its effectors removed, its scripts killed (the ride's own one quietly while the world is shutting down) and
// the UI's selection cleared if it is the ride; then the thing's own destroy.
void CObject::FinalDestroy() {
    GetControl()->mNumberInPark -= 1;
    gWorld->Unregister(this);
    CMsgRideDeleted message(mBase.GetThingId());
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->PostMessage(message);
    const uint32_t value = GetCurrentResidualValue();
    gWorld->GetBankAccount()->MakeDeposit(value);
    if ((mFlags & 0x10) != 0) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorSecurity, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
    }
    if ((mFlags & 0x80) != 0 && RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0) < 2) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorFireworks, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
    }
    if ((mFlags & 1) != 0) {
        if (IsDirty() != 0) {
            gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(6), tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
        } else {
            gWorldMap->RemoveRegionalEffector(kRegionEffectorRelief, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
        }
    }
    if (mF50 != 0) SCR_KillScript(static_cast<int32_t>(mF50));
    if (gWorld->mF1DA738 == 2) RSSE_KillScript(static_cast<int32_t>(mF20), 0);
    else RSSE_KillScript(static_cast<int32_t>(mF20), gActionRecorder.mRecording != 0 ? 4 : 7);
    if (gUIInterface.GetSelectedObject() == this) gUIInterface.SetSelectedObject(nullptr);
    mBase.FinalDestroy();
}

CMsgRideDeleted::~CMsgRideDeleted() {}

// The ride's open and close steps, whether it runs, may be chosen or upgraded, its operating speed, duration and capacity, the blocked exit and the messages
// it receives (main app, Object.cpp range; unit w200). The members are the Mac's CObject members; the names of the fields are the header's. The ride is
// closed while mF64 is 0, running while its script's variable 9 is above 0, and its state word mState says whether it wants an upgrade.

namespace {
// The strings of the string pool at 0x1024294c (the offset after each). The staff member text has a %d that the original passes no number for (it prints
// whatever the register held); the host passes nothing either. "Cannot upgrade this object" has no %d but the original passes the id.
constexpr const char* kStaffGone = "Object %d: staff member assigned to service me no longer exists\n";   // +0x2a6
constexpr const char* kSpeed = "SPEED = %d\n";                                                             // +0x2e7
constexpr const char* kDuration = "DUR = %d\n";                                                            // +0x2f3
constexpr const char* kCapacity = "CAPACITY = %d\n";                                                       // +0x2fd
constexpr const char* kClosing = "Object %d: Closing...";                                                  // +0x699
constexpr const char* kPersonLoaded = "Object %d: person being loaded was %d (about to zeroise)\n";       // +0x6af
constexpr const char* kNonOpenable = "!!!!!!!!!! Opening non-openable ride! (non-fatal, can ignore) !!!!!!!!!! \n";   // +0x6e9
constexpr const char* kWantsMaintenance = "Object %d: wants maintenance\n";                                // +0x781 (the Mac's text for an upgrade request)
constexpr const char* kCannotUpgrade = "Cannot upgrade this object\n";                                     // +0x7d5
constexpr const char* kCancelling = "Object %d: Cancelling request for upgrade\n";                         // +0x7f1
constexpr const char* kNeverRequested = "Object %d: Never requested upgrading in the first place\n";       // +0x81c

constexpr uint32_t kTrace = 2;            // the level of the open, close and upgrade traces
constexpr uint32_t kSettingTrace = 0x7c;  // the level of the operating settings and of the upgrade request's first line
constexpr uint16_t kNoMapCell = 0;        // the halfword at 0x102a0bb6, 0 (GetBackOfQueue answers it for a ride without a queue end)

constexpr int32_t kStaffDeath = 0x1b;     // the message type CMsgDeath answers
constexpr int32_t kMonthEnd = 0xb;        // the message type that closes the six histories at the end of a month (the class that carries it is not known)

// The message centre the Mac builds on first use (src/BankAccount.cpp has the same helper).
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The ride record of the ride's object type (a call each time: the Mac fetches it again for every field it reads).
RideInfo* InfoOf(const CObject& ride) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride.mObjectType});
}

int32_t ThingId(const CObject& ride) {
    return static_cast<int32_t>(ride.mBase.GetThingId().mIndex);
}
}  // namespace

// 0x10107ba0 (256 bytes). The type comes from the message's own GetType (a virtual call). A staff member's death (a CMsgDeath) takes him off the ride when he
// is the one assigned. The month end (type 0xb) closes the books of the six histories: each one's temp word is stored and then zeroed.
void CObject::ReceiveMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    if (type == kStaffDeath) {
        if (static_cast<const CMsgDeath&>(message).mThingId == mF5A) {
            CTRACE(kTrace, kStaffGone);
            UnassignStaffMember();
        }
    } else if (type == kMonthEnd) {
        mHistory1A4.SetFromTemp();
        mHistory1A4.mTemp = 0;
        mHistory22C.SetFromTemp();
        mHistory22C.mTemp = 0;
        mHistory2B4.SetFromTemp();
        mHistory2B4.mTemp = 0;
        mHistory6C.SetFromTemp();
        mHistory6C.mTemp = 0;
        mHistoryF4.SetFromTemp();
        mHistoryF4.mTemp = 0;
        mHistory33C.SetFromTemp();
        mHistory33C.mTemp = 0;
    }
}
// 0x10107cd4 (56 bytes).
RideInfo* CObject::GetDBInfo() const {
    return InfoOf(*this);
}

// Makes (flag 0) or restores (flag nonzero) the ride's instance in the engine from the ride record (nothing when the database has none). A ride
// whose record has "don't apply offset" set (the kind 0x361) is placed at the record's engine offset, else at the balance sheet's; the other
// kind (0x32f) is placed at the map cell mF30 (1 based: x is the low seven bits, z the rest, 0 wrapping). Only the 0x32f kind tells the instance
// its script and puts the coaster queues back when it is restored; a made instance's number goes to mF1C.
void CObject::UpdateEngine(int32_t flag) {
    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
    if (info == nullptr) return;
    sFixedRideParams params;
    params.mRotation = mRotation;
    params.mF0C = info->Value(kRideSlot_Shape).mShape;
    if (info->Value(kRideSlot_DontApplyOffset).mInteger != 0) {
        const int32_t x = info->Value(kRideSlot_EngineMapOffsetOverrideX).mInteger;
        params.mX = x != 0 ? x : gMainBalance.mValues[kMainBalanceRideOffsetX].mInteger;
        const int32_t z = info->Value(kRideSlot_EngineMapOffsetOverrideY).mInteger;
        params.mZ = z != 0 ? z : gMainBalance.mValues[kMainBalanceRideOffsetZ].mInteger;
        if (flag != 0) {
            RestoreRideInstance(mF1C, info->mRideTypeNumber, 0x361, &params, 0, this,
                                info->Value(kRideSlot_EngineFootprintWidthOverride).mInteger,
                                info->Value(kRideSlot_EngineFootprintHeightOverride).mInteger, static_cast<int32_t>(mF20));
        } else {
            mF1C = InstanceRide(info->mRideTypeNumber, 0x361, &params, 0, static_cast<int32_t>(mF20), this,
                                info->Value(kRideSlot_EngineFootprintWidthOverride).mInteger,
                                info->Value(kRideSlot_EngineFootprintHeightOverride).mInteger, 0xffffffffu, 0);
        }
    } else {
        params.mX = static_cast<int32_t>((static_cast<uint32_t>(mF30) - 1u) & 0x7fu);
        params.mZ = static_cast<int32_t>((static_cast<uint32_t>(mF30) - 1u) >> 7);
        if (flag != 0) {
            RestoreRideInstance(mF1C, info->mRideTypeNumber, 0x32f, &params, 0, this, 0, 0, 0);
            SetRideScriptID(mF1C, mF20);
            HACKRestoreCoasterQueues(mF1C);
        } else {
            mF1C = InstanceRide(info->mRideTypeNumber, 0x32f, &params, 0, static_cast<int32_t>(mF20), this, 0, 0, 0xffffffffu, 0);
        }
    }
}

// A ride's takings, costs, prizes and satisfaction (main app, Object.cpp range; unit w198): the 29 CObject members between 0x10107f50 and
// 0x101105f4 that count money and customers (GetObjectAge 0x10107f50, GetRemainingLife 0x10108024, WalkedAway 0x1010eec4, HadCustomer 0x1010ef0c,
// RecordNewTakings 0x1010ef54, RecordACost 0x1010f34c, HadAWinner 0x1010f4b8, GetPricePerUse 0x1010f4f4, GetPrizeValue 0x1010f52c, SetPrizeValue
// 0x1010f564, GetProspectiveCostOfGoods 0x1010f598, GetCostOfGoods 0x1010f6ec, GetTotalRevenue 0x1010f894, GetProfitability 0x1010f8d4,
// ExpressSatisfaction 0x1010f974, GetCustomerSatisfaction 0x1010f9d0, GetPercentageOfWinners 0x1010fa40, GetProductQuality 0x1010fb38,
// SetProductQuality 0x1010fb78, GetChanceOfWinning 0x1010fbb8, SetChanceOfWinning 0x1010fbf8, SetAmountOfSpecialIngredient 0x1010fc38,
// GetAmountOfSpecialIngredient 0x1010fc7c, GetSpecialIngredientName 0x1010fcc4, GetResidualValuePercentage 0x1010fd98, GetCurrentResidualValue
// 0x1010ffc8, PersonUsedToilet 0x10110070, WillIWin 0x101103e0 and SetPricePerUse 0x101105f4). src/Object.cpp, src/Object_turn.cpp,
// src/Object_queries.cpp and src/Object_queue.cpp hold other CObject members. The meanings of the fields are mine (the header says which).

namespace {
constexpr int32_t kHistoryCount = 30;                       // every average and sum here asks for the last 30
constexpr int64_t kTicksPerDay = 864000000000LL;            // 100 ns ticks (src/Staff_queries.cpp)
constexpr int64_t kTicksPerMonth = 30 * kTicksPerDay;       // 0x1792f8648000: a month is 30 days here
constexpr int64_t kTicksPerYear = 365 * kTicksPerDay;       // 0x11ed178c6c000
constexpr int32_t kChallengeShopTakings = 12;               // the challenge counters AddToCounter is given (values from the listings)
constexpr int32_t kChallengeSideshowTakings = 13;
constexpr uint32_t kUpgradeStride = 16;                     // ride record slots between two upgrade levels
constexpr uint16_t kRegionEffectorDirty = 6;                // the effector a dirty toilet adds (no name in RegionEffector yet)
constexpr uint32_t kTextTableIndex = 4;                     // gText.mResourcePointers[4], the Mac's gText + 0x55c

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The ride record of the object's type (every use calls GetRideInfo again, like the original).
RideInfo* RecordOf(const CObject* object) { return gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType)); }

// The cell the effectors are added to: the object's own cell as a 1 based map id.
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellOf(const CObject* object) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(object->mBase.mCellX + object->mBase.mCellY * 0x80 + 1);
    return tag;
}

// The null ids every AddToCounter call is given (the Mac loads two zero halfwords from the stack as whole words).
void AddToCounter(ChallengeType type, int32_t amount) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> noObject;
    noObject.mIndex = 0;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> noThing;
    noThing.mIndex = 0;
    gWorld->GetChallengeHQ()->AddToCounter(type, amount, noObject, noThing);
}

// The time a ride has been built, as the world's funny time minus the build stamp (the object is read as a CWorldSavvy: GetWorld ignores `this`).
int64_t BuiltFor(const CObject* object) {
    static_assert(sizeof(TbTimeStamp) == sizeof(object->mBuilt), "a time stamp is two words");
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(object);
    TbTimeStamp now = savvy->GetWorld().mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // The original passes the member's own address (the stamp is read in place).
    TbTimeDiff span = now - *reinterpret_cast<const TbTimeStamp*>(object->mBuilt);
    return span.mTicks;
}

// The sum of the last 30 of a history, converted the way __cvt_fp2unsigned does.
uint32_t SumOf(const CHistory<uint32_t, 30>& history) {
    double sum;
    history.GetSumOfLast(sum, kHistoryCount);
    return cw_cvt_fp2unsigned(sum);
}

// Dirty: the flag for "provides relief" is set and the cleanliness, truncated to a byte, is below 25.
bool IsDirtyNow(const CObject* object) {
    if ((object->mFlags & 1) == 0) return false;
    const float cleanliness = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(object->mF40))));
    return cleanliness < 25.0f;
}

// Both cost-of-goods functions: each term is (value - 50) / 200 clamped to [-0.5, 0.5] (the compares are ordered ones that let a NaN through), the
// ingredient term changes sign for the record's special ingredients 1 and 3, and the cost scales the stored prize word by (1 + quality) + ingredient.
float ClampTerm(float term) {
    if (term < -0.5f) return -0.5f;
    if (term > 0.5f) return 0.5f;
    return term;
}

int32_t CostOfGoods(const CObject* object, float quality, float ingredient) {
    const uint32_t goods = object->mCostOfGoods;
    const float qualityTerm = ClampTerm((quality - 50.0f) / 200.0f);
    float ingredientTerm = ClampTerm((ingredient - 50.0f) / 200.0f);
    const int32_t special = RecordOf(object)->Value(kRideSlot_SpecialIngredient).mInteger;
    if (special == 1 || special == 3) ingredientTerm = ingredientTerm * -1.0f;
    const float scaled = static_cast<float>(goods) * ((1.0f + qualityTerm) + ingredientTerm);
    return ppc_fctiwz(static_cast<double>(scaled));
}

// The ride record's scrap value of one year, for the upgrade level the ride is at (the slot of level 0 plus 16 slots per level).
int32_t ScrapValue(const RideInfo* record, RideBalanceSlot year, uint8_t level) {
    return record->mBalance.mValues[year + kUpgradeStride * level].mInteger;
}
}  // namespace

// The whole days since the ride was built (the 64 bit span divided by a day, the low word kept).
int32_t CObject::GetObjectAge() const { return static_cast<int32_t>(BuiltFor(this) / kTicksPerDay); }

// mF44 (the remaining life) truncated.
int32_t CObject::GetRemainingLife() const { return ppc_fctiwz(static_cast<double>(mF44)); }

// 0x10108068 (104 bytes).
void CObject::SetOperatingSpeed(uint32_t speed) {
    RSSE_SetRideSpeed(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), static_cast<int32_t>(speed));
    CTRACE(kSettingTrace, kSpeed, static_cast<int32_t>(speed));
    mF54 = speed;
}

// 0x10108104 (272 bytes). The byte is held between the record's minimum and maximum duration (signed compares; a bound is fetched from the record each time
// it is read: 2 fetches inside the range or below it, 3 above it) and masked to a byte again. Then the script's variable 3 and mF58.
void CObject::SetOperatingDuration(uint8_t duration) {
    int32_t value = duration;
    if (value >= InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger) {
        if (value > InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger) value = InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger;
    } else {
        value = InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger;
    }
    value &= 0xff;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 3, value);
    CTRACE(kSettingTrace, kDuration, value);
    mF58 = static_cast<uint8_t>(value);
}

// 0x10108248 (356 bytes). The same with the record's capacity bounds, but only when their sum is above 0 (the sum wraps; the first fetch gives the maximum,
// the second the minimum; the clamp then fetches again: 4 fetches inside the range or below it, 5 above it, 2 when the sum is not above 0). Then the
// script's variable 2 and mF59.
void CObject::SetOperatingCapacity(uint8_t capacity) {
    const int32_t maximum = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
    const int32_t minimum = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
    int32_t value = capacity;
    if (static_cast<int32_t>(static_cast<uint32_t>(minimum) + static_cast<uint32_t>(maximum)) > 0) {
        if (value >= InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger) {
            if (value > InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger) value = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
        } else {
            value = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
        }
    }
    value &= 0xff;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 2, value);
    CTRACE(kSettingTrace, kCapacity, value);
    mF59 = static_cast<uint8_t>(value);
}

// 0x101083e0 (396 bytes). A choosable ride that is working, open, has a queue end and room in its queue (and, for a coaster, a usable track). The ride
// record is fetched twice at the end: a record whose WhichTrackType is 1 refuses the ride while mF28 is 0, and (the second fetch) so does a 2.
int32_t CObject::IsAvailableToChoose() {
    if ((mFlags & 4) == 0) return 0;
    if (IsBrokenDown()) return 0;
    if (IsClosed()) return 0;
    if (IsCondemned() != 0) return 0;
    if (GetBackOfQueue().mIndex == kNoMapCell) return 0;
    if (HasFreeQueueSpace() == 0) return 0;
    if (IsCoaster() != 0) {
        if (IsCoasterUseable(this) == 0) return 0;
    }
    if (InfoOf(*this)->Value(kRideSlot_WhichTrackType).mInteger == 1) {
        if (mF28 == 0) return 0;
    }
    if (InfoOf(*this)->Value(kRideSlot_WhichTrackType).mInteger == 2) {
        if (mF28 == 0) return 0;
    }
    return 1;
}

// CObject, main app, unit w202 (the Mac files them under Object.cpp, the pieces 0x101085a0 to 0x10108a0c and 0x1010d0e0 to 0x1010ee94): the ride judges
// its queue (HasFreeQueueSpace, GetIdealQueueLength, QueueTooLong), admits a guest (AdmitPerson, FinishedAdmitting, ReadyToAdmit,
// PersonLeftDuringAdmittance), takes its guests on and off (LetPeopleOn, LetPeopleOff, ProcessStateFunctioning) and shows the first person view
// (GoFirstPerson). src/Object.cpp, src/Object_turn.cpp, src/Object_queries.cpp, src/Object_queue.cpp, src/Object_takings.cpp, src/Object_upkeep.cpp
// and src/Object_opening.cpp hold other CObject members. The guests are CPerson, a placeholder class: the ride reads one word of a guest (+0x220, his
// state) and calls five of his members, which stay untranslated (the launcher answers zero).

int32_t ObjectReadyForAdmitting(CPerson*) __asm__("__ZNK7CPerson17ReadyForAdmittingEv");   // 0x1011f0b8
void ObjectOnAdmittance(CPerson*) __asm__("__ZN7CPerson12OnAdmittanceEv");                 // 0x1011f114
int32_t ObjectExitRide(CPerson*) __asm__("__ZN7CPerson8ExitRideEv");                       // 0x1011f5a4

// 0x1034c35c: the float GetIdealQueueLength and QueueTooLong store the ratio of the ride's speed to its record's initial speed in (the name is mine).
float gObjectQueueSpeedRatio = 0.0f;

namespace {
using ThingId_2 = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The strings of the string pool at 0x1024294c (the offset after each). The ones without a line end are the Mac's.
constexpr const char* kNoQueueConstant = "No queue constant entered in SAM file for object number %d at upgrade level %d\n";   // +0x30c
constexpr const char* kZeroDuration = "Operating duration set to zero - shouldn't have a zero duration for a ride!";          // +0x35c
constexpr const char* kForcing = "Object %d: script admitted person %d but he doesn't know yet, forcing him onto ride\n";     // +0x8a0
constexpr const char* kCannotAdmit = "Object %d (type %d): cannot admit person %d, broke down or closed!";                    // +0x8f5
constexpr const char* kLoadedIs = "Object %d (type %d): mPersonBeingLoaded is %d, but admitting person %d (%d)\n";             // +0x938
constexpr const char* kWrongPerson = "admitting wrong person - check debug output\n";                                         // +0x985
constexpr const char* kAdmitting = "Object %d::AdmitPerson - person being loaded is %d\n";                                    // +0x9b2
constexpr const char* kChangedMind = "Object %d (type %d): cannot admit person %d, script changed its mind about admission! (%d)";   // +0x9e6
constexpr const char* kNotExpecting = "Person %d: telling object %d about leaving during admittance, but object not expecting me (expecting %d instead)\n";   // +0xa41
constexpr const char* kLeftAfter = "Object %d: person %d left after I'd admitted him.  Fine, stuff you then.";                // +0xab3
constexpr const char* kLetMeOn = "Object %d: person %d is in LETMEON but wants to leave.  Zeroing LETMEON\n";                 // +0xafc
constexpr const char* kZeroCapacity = "Object has zero capacity\n";                                                           // +0xbcd
constexpr const char* kLoading = "Object %d::LetMeOn - person being loaded is %d\n";                                          // +0xbe7
constexpr const char* kThinksOtherwise = "Object %d thinks person %d is being loaded, person thinks otherwise\n";             // +0xc17
constexpr const char* kNotOnRide = "Object %d: trying to dismiss person %d, but they're not on this ride!";                   // +0xc5c
constexpr const char* kBreakingDown = "Object %d: Setting state BROKEN_DOWN\n";                                               // +0xca2
constexpr const char* kCondemning = "Object %d: Setting state CONDEMNED\n";                                                   // +0xcc8

constexpr uint16_t kNobodyLoading = 0;      // 0x102a0be6: LetPeopleOn, nobody being loaded
constexpr uint16_t kNobodyQueued = 0;       // 0x102a0be8: LetPeopleOn, nobody at the head of the queue
constexpr uint16_t kNobodyLoaded = 0;       // 0x102a0bea: LetPeopleOn, the check of the person being loaded
constexpr uint16_t kNobodyToLetOff = 0;     // 0x102a0bec: LetPeopleOff and ProcessStateFunctioning, nobody to let off

constexpr uint32_t kGuestState = 0x220;     // the offset of a guest's state word
constexpr uint32_t kBeingLoaded = 0xd;      // the guest is being loaded
constexpr uint32_t kAdmittedUnaware = 0xe;  // the guest is admitted and does not know it yet
constexpr uint32_t kOnTheRide = 0x10;       // the guest is on the ride

// The slot of the record's upgrade array for the ride's upgrade level: 16 slots (0x40 bytes) to a level.
RideBalanceSlot AtLevel(RideBalanceSlot slot, uint8_t level) {
    return static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16);
}

// The guest of a thing id: the Mac asserts that row 0 of the thing array is empty before every fetch.
CPerson* GuestOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return static_cast<CPerson*>(gThingArray[id].mThing);
}

ThingId_2 IdOfGuest(CPerson* guest) {
    return reinterpret_cast<CThing*>(guest)->GetThingId();
}

// The ideal length of the queue of a ride that has one (mFlags 8), the steps GetIdealQueueLength and QueueTooLong share (the Mac repeats them in each).
// The speed ratio (the operating speed over the record's initial speed of the upgrade level, 1 for a ride without a speed) goes to the global, which
// is read back for the length: the queue wait constant of the level times the ratio times the capacity, over the duration, at least 4, as a word.
uint32_t IdealLength(const CObject& ride) {
    if (ride.mF54 != 0) {
        const float speed = static_cast<float>(ride.mF54);
        const float initial = static_cast<float>(InfoOf(ride)->Value(AtLevel(kRideSlot_Upgrades_InitSpeed, ride.mF4C)).mInteger);
        gObjectQueueSpeedRatio = speed / initial;
    } else {
        gObjectQueueSpeedRatio = 1.0f;
    }
    const float constant = InfoOf(ride)->Value(AtLevel(kRideSlot_Upgrades_QueueWaitTimeConstant, ride.mF4C)).mFloat;
    GAME_ASSERTMSG(constant > 0.0f, kNoQueueConstant, ride.mObjectType, ride.mF4C);
    GAME_ASSERTMSG(ride.mF58 != 0, kZeroDuration);
    const float capacity = static_cast<float>(ride.mF59);
    const float duration = static_cast<float>(ride.mF58);
    const float length = (constant * (gObjectQueueSpeedRatio * capacity)) / duration;
    return cw_cvt_fp2unsigned(static_cast<double>(length > 4.0f ? length : 4.0f));
}

// The steps LetPeopleOff takes once it has the ride's script (ProcessStateFunctioning has them inline): the script's variable 1 names the guest to let
// off; he must be on the ride, and the variable is cleared when he has gone.
void LetGuestOff(const CObject& ride, RSSE_SCRIPT* script) {
    const uint16_t id = static_cast<uint16_t>(RSSE_Getvar(script, 1));
    if (id == kNobodyToLetOff) return;
    CPerson* guest = GuestOf(id);
    const ThingId_2 personId = IdOfGuest(guest);
    const ThingId_2 rideId = ride.mBase.GetThingId();
    GAME_ASSERTMSG(WordAt_2(guest, kGuestState) == kOnTheRide, kNotOnRide, rideId.mIndex, personId.mIndex);
    if (ObjectExitRide(guest) != 0) RSSE_Setvar(script, 1, 0);
}

// Whether LetPeopleOn may load the queue's head, once the script's variable 0 is 0: the load (variable 5) is below the capacity (variable 2), or the
// ride is a coaster or a track ride of type 2; the ride runs continuously or its variable 9 is 0; nobody is being loaded. Each call in the Mac's order.
bool MayLoadNext(const CObject& ride, RSSE_SCRIPT* script) {
    const int32_t capacity = RSSE_Getvar(script, 2);
    const int32_t load = RSSE_Getvar(script, 5);
    if (load >= capacity) {
        if (ride.IsCoaster() == 0) {
            if (InfoOf(ride)->Value(kRideSlot_WhichTrackType).mInteger != 2) return false;
        }
    }
    if (RSSE_Getvar(script, 9) != 0 && (ride.mFlags & 0x100) == 0) return false;
    return ride.mPersonLoaded == kNobodyLoading;
}
}  // namespace

// 0x101085a0 (80 bytes). The queue is full when it holds four people to each unit of mF3C (an unsigned compare). The Mac answers a byte.
int32_t CObject::HasFreeQueueSpace() const {
    return static_cast<uint32_t>(GetNumPeopleInQueue()) < (mF3C << 2);
}

// 0x10108624 (448 bytes). 100 for a ride without a queue.
uint32_t CObject::GetIdealQueueLength() const {
    if ((mFlags & 8) == 0) return 100;
    return IdealLength(*this);
}

// 0x10108818 (484 bytes). The queue is too long when the people in it reach the ideal length (100 for a ride without a queue); the count is asked last.
bool CObject::QueueTooLong() const {
    uint32_t ideal = 100;
    if ((mFlags & 8) != 0) ideal = IdealLength(*this);
    return static_cast<uint32_t>(GetNumPeopleInQueue()) >= ideal;
}

// CObject, main app, unit w197 (the Mac files them under Object.cpp, address range 0x10108a28 to 0x1010a488; the other CObject members are in
// src/Object.cpp and src/Object_turn.cpp): the ride's queue. Fifteen members: the list of queued people (AddPersonToQueue, RemovePersonFromQueue,
// GetNumPeopleInQueue, GetPositionInQueue), the line of queue cells (GetFrontOfPhysicalQueue, GetBackOfPhysicalQueue, GetBackOfVirtualQueue,
// GetBackOfQueue, GetQCellBehind, RecalculateBackOfQueue, IsBOQConnected, IsExitCellConnected) and the queue positions turned into map positions
// (ConvertQPositionToMapPos, ConvertPhysicalQPositionToMapPos, ConvertVirtualQPositionToMapPos). CPerson is a placeholder class here, so the two
// link halfwords of a person (+0x228 the person behind, +0x22a the person ahead) are read through file-local accessors, as src/Object_turn.cpp does.

// CPerson::EvaluateQueueEdit (0x1011f2f8, untranslated), by its Mac name as src/Object_turn.cpp does for its CPerson members.
void ObjectEvaluateQueueEdit(CPerson*) __asm__("__ZN7CPerson17EvaluateQueueEditEv");

namespace {
using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr const char* kNotRemoved = "Person not successfully removed from queue\n";                        // +0x439
constexpr std::size_t kBehind = 0x228;   // a person's link to the person behind in the queue
constexpr std::size_t kAhead = 0x22a;    // a person's link to the person ahead in the queue
constexpr uint32_t kCellSize = 0x44;

uint16_t HalfAt(const void* base, std::size_t offset) {
    uint16_t half;
    std::memcpy(&half, static_cast<const char*>(base) + offset, sizeof(half));
    return half;
}

void SetHalfAt(void* base, std::size_t offset, uint16_t value) { std::memcpy(static_cast<char*>(base) + offset, &value, sizeof(value)); }

// The thing table's pointer for an id; every dereference of one is preceded by the same game assertion on the table's first row.
char* Guest(uint16_t id) { return static_cast<char*>(gThingArray[id].mThing); }
void CheckRowZero() { GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero); }

CMapCell* CellAt(CMap* map, int32_t id) { return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(map) + (id - 1) * static_cast<int32_t>(kCellSize)); }

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint8_t PositionByte(uint32_t position) {
    const float quarter = static_cast<float>(position) * 0.25f;
    const float scaled = 255.0f * quarter;
    return static_cast<uint8_t>(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled))) & 0xff);
}

constexpr uint8_t kDirections[4] = {1, 4, 0x10, 0x40};
}  // namespace

// Appends the person to the end of the list (the head is mQueuedPerson, each person links to the one behind at +0x228) and links the person back to
// the one that was last (+0x22a; 0 when the queue was empty).
void CObject::AddPersonToQueue(ThingTag person) {
    CTRACE(2, "Object %d adding person %d to queue\n", mBase.GetThingId().mIndex, person.mIndex);
    uint16_t last = mQueuedPerson;
    if (last == kNoThing) {
        mQueuedPerson = person.mIndex;
    } else {
        for (;;) {
            CheckRowZero();
            if (HalfAt(Guest(last), kBehind) == kNoThing) break;
            CheckRowZero();
            last = HalfAt(Guest(last), kBehind);
        }
        CheckRowZero();
        SetHalfAt(Guest(last), kBehind, person.mIndex);
    }
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kAhead, last);
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kBehind, kNoThing);
}

// Takes the person out of the list (the neighbours link to each other, or the head moves on), clears the ride script's variable 0 when it names the
// person, and asserts that both of the person's links are clear.
void CObject::RemovePersonFromQueue(ThingTag person) {
    CTRACE(2, "Object %d removing person %d from queue\n", mBase.GetThingId().mIndex, person.mIndex);
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (static_cast<uint32_t>(RSSE_Getvar(script, 0)) == person.mIndex) RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 0);
    CheckRowZero();
    const uint16_t ahead = HalfAt(Guest(person.mIndex), kAhead);
    CheckRowZero();
    const uint16_t behind = HalfAt(Guest(person.mIndex), kBehind);
    if (ahead == kNoThing) {
        mQueuedPerson = behind;
    } else {
        CheckRowZero();
        SetHalfAt(Guest(ahead), kBehind, behind);
    }
    if (behind != kNoThing) {
        CheckRowZero();
        SetHalfAt(Guest(behind), kAhead, ahead);
    }
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kAhead, kNoThing);
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kBehind, kNoThing);
    CheckRowZero();
    GAME_ASSERTMSG(kNoThing == HalfAt(Guest(person.mIndex), kBehind), kNotRemoved);
    CheckRowZero();
    GAME_ASSERTMSG(kNoThing == HalfAt(Guest(person.mIndex), kAhead), kNotRemoved);
}

// The length of the list: the position of nobody.
int32_t CObject::GetNumPeopleInQueue() const { return GetPositionInQueue(ThingTag{kNoThing}); }

// The position of the person in the list (0 for the head), or the length of the list for nobody. The walk follows the +0x228 links of people who are
// in the queueing state; a person who is not (or a thing that is gone) ends it. A person who is not found is logged, asserted and answered -1.
// A row with no thing leaves the walk where it is (the original loops for ever; kept).
int32_t CObject::GetPositionInQueue(ThingTag person) const {
    int32_t count = 0;
    uint16_t current = mQueuedPerson;
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(&mBase);
    while (current != person.mIndex && current != kNoThing) {
        count++;
        savvy->GetWorld();
        CheckRowZero();
        CPerson* guest = reinterpret_cast<CPerson*>(Guest(current));
        if (guest != nullptr && guest->InQueueingState() != 0) {
            current = HalfAt(guest, kBehind);
        } else if (guest != nullptr) {
            current = kNoThing;
        }
    }
    if (current == kNoThing && person.mIndex != kNoThing) {
        const uint32_t turn = savvy->GetWorld().mTurn;
        CTRACE(2, "Object %d (type %d): Could not find person %d in the queue (%d)\n", mBase.GetThingId().mIndex, mObjectType, person.mIndex, turn);
        GAME_ASSERTMSG(0, "GetPositionInQueue: Could not find the thing in the queue\n");
        count = -1;
    }
    return count;
}

// The cell in front of the queue: the neighbour of the entry cell (mF32) in the first direction (up, down, left, right) it connects to, 0 when it
// connects to none.
MapTag CObject::GetFrontOfPhysicalQueue() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(&mBase);
    CMapCell* cell = CellAt(savvy->GetMap(), mF32);
    const uint8_t neighbours = cell->GetNeighbours();
    const MapDelta* delta;
    if ((neighbours & 1) != 0) {
        delta = &gMapDeltaMinusY;
    } else if ((neighbours & 0x10) != 0) {
        delta = &gMapDeltaPlusY;
    } else if ((neighbours & 0x40) != 0) {
        delta = &gMapDeltaMinusX;
    } else if ((neighbours & 4) != 0) {
        delta = &gMapDeltaPlusX;
    } else {
        return MapTag{kNoThing};
    }
    return MapTag{static_cast<uint16_t>(mF32 + delta->mX + static_cast<int32_t>(static_cast<uint32_t>(delta->mY) << 7))};
}

MapTag CObject::GetBackOfQueue() {
    if ((mFlags & 8) != 0) return GetBackOfPhysicalQueue();
    return GetBackOfVirtualQueue();
}

// The last cell of the queue's line (the cache mF36, 0 when stale): walks back from the front cell, at most 1000 cells, counting them in mF3C. A line
// that long is logged as a crash and answered 0.
MapTag CObject::GetBackOfPhysicalQueue() {
    int32_t counter = 1000;
    if (mF36 == kNoThing) {
        const MapTag front = GetFrontOfPhysicalQueue();
        mF36 = front.mIndex;
        mF3C = 0;
        MapTag current = front;
        while (current.mIndex != kNoThing && (counter-- > 0)) {
            mF36 = current.mIndex;
            current = GetQCellBehind(current);
            mF3C = mF3C + 1;
        }
        if (counter < 1) {
            CTRACE(0xc, "*** GetBackOfQueue() crashed! ***\n");
            return MapTag{kNoThing};
        }
        return MapTag{mF36};
    }
    return MapTag{mF36};
}

// Both flag cases call the physical one.
MapTag CObject::GetBackOfVirtualQueue() { return GetBackOfPhysicalQueue(); }

// Forgets the cached back, finds it again, stamps the back cell on the map, tells everybody in the queue (except the one being loaded) to evaluate
// again, logs whether the back is connected, reopens a ride that was closed and can open, and clears mF5A.
void CObject::RecalculateBackOfQueue() {
    mF36 = kNoThing;
    GetBackOfPhysicalQueue();
    CTRACE(2, "Object's queue is now %d cells long\n", mF3C);
    CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8);
    const uint32_t stamp = map->MakeNewTimeStamp();
    const MapTag back = GetBackOfPhysicalQueue();
    map->SetTimeStamp(stamp, back);
    CTRACE(2, "Telling people in queue to reevaluate");
    uint16_t current = mQueuedPerson;
    while (current != kNoThing) {
        CheckRowZero();
        CPerson* guest = reinterpret_cast<CPerson*>(Guest(current));
        const uint16_t next = HalfAt(guest, kBehind);
        if (current != mPersonLoaded) ObjectEvaluateQueueEdit(guest);
        current = next;
    }
    const int32_t connected = IsBOQConnected();
    CTRACE(2, "Back of queue is %sconnected\n", connected != 0 ? "" : "not ");
    if (IsClosed() && IsOpenable() != 0 && (IsTrackRide() == 0 || mF28 != 0)) Reopen();
    mF5A = 0;
}

// Whether the back of the queue joins up: a queue with cells, when the back cell's direction differs from its neighbour byte; a queue without, when the
// entry cell and its neighbour across the ride's rotation connect to each other.
int32_t CObject::IsBOQConnected() {
    if ((mFlags & 8) != 0) {
        const MapTag back = GetBackOfPhysicalQueue();
        if (back.mIndex == kNoThing) return 0;
        CMapCell* cell = CellAt(gWorldMap, back.mIndex);
        return cell->GetDirection() != cell->GetNeighbours() ? 1 : 0;
    }
    CMapCell* cell = CellAt(gWorldMap, mF32);
    uint8_t direction = 0;   // the original leaves it unset for a rotation other than 0, 90, 180 and 270
    if (mRotation == 0) {
        direction = 0x10;
    } else if (mRotation == 90) {
        direction = 4;
    } else if (mRotation == 180) {
        direction = 1;
    } else if (mRotation == 270) {
        direction = 0x40;
    }
    const uint8_t opposite = OPPOSITE(direction);
    CMapCell* across = cell->GetNeighbouringCell(opposite);
    const bool joined = across->IsPath() != 0 && (across->GetNeighbours() & direction) != 0;
    return joined && (cell->GetNeighbours() & opposite) != 0 ? 1 : 0;
}

// Whether the exit cell (mF34) has a way out: from the neighbour across its direction (turned round when the exit is the entry), the first of the four
// sides that is not the opposite and has no wall in the way. The original mixes the direction bits (1, 4, 0x10, 0x40) with the index i.
int32_t CObject::IsExitCellConnected() {
    CMapCell* cell = CellAt(gWorldMap, mF34);
    uint8_t direction = cell->GetDirection();
    if (mF34 == mF32) direction = OPPOSITE(direction);
    CMapCell* next = cell->GetNeighbouringCell(direction);
    const int32_t id = next->mCellId;
    for (int32_t i = 0; i < 4; i++) {
        const uint8_t opposite = OPPOSITE(direction);
        if (kDirections[i] != opposite &&
            MAP_wall_in_way(static_cast<uint8_t>((id - 1) & 0x7f), static_cast<uint8_t>((static_cast<uint32_t>(id - 1) >> 7) & 0xff), static_cast<uint8_t>(i),
                            static_cast<NavMode>(0)) == 0) {
            CTRACE(2, "Connected in direction %d\n", i);
            return 1;
        }
    }
    return 0;
}

// The queue cell behind the cell: the first neighbour (up, down, right, left) that is a queue cell, not an entrance, whose direction is the one that
// leads to this side (0x10, 1, 0x40, 4), else 0. Reads the game world's map; no `this`.
MapTag CObject::GetQCellBehind(MapTag cell) {
    CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8);
    CMapCell* here = CellAt(map, cell.mIndex);
    const MapDelta* deltas[4] = {&gMapDeltaMinusY, &gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaMinusX};
    const uint8_t wanted[4] = {0x10, 1, 0x40, 4};
    for (int32_t i = 0; i < 4; i++) {
        MapDelta delta = *deltas[i];
        CMapCell* neighbour = here->GetNeighbouringCell(&delta, 1);
        if (neighbour != nullptr && neighbour->IsQueue() != 0 && neighbour->IsEntrance() == 0 && neighbour->GetDirection() == wanted[i]) {
            return MapTag{neighbour->mCellId};
        }
    }
    return MapTag{kNoThing};
}

// A queue position (0.25 of a cell each) to a map position: a ride with a queue line uses the physical conversion, the other the virtual one (positions
// 0 to 3, asserted).
int32_t CObject::ConvertQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    if ((mFlags & 8) != 0) return ConvertPhysicalQPositionToMapPos(position, cell, x, y);
    GAME_ASSERTMSG(position < 4 ? 1 : 0, "Virtual queue problem!\n");
    return ConvertVirtualQPositionToMapPos(position, cell, x, y);
}

// The cell is the queue cell `position / 4` back from the front (0 once the line ends); the place in it follows its direction: the byte
// 255 * position / 4 (of the remainder) along it and a random 0x72 to 0x8d across. In the second half of the cell the direction is the one of the cell
// behind (or, with none, the side of the first path neighbour, else the cell's own direction) and the byte is shifted by 0x80 for direction 4.
int32_t CObject::ConvertPhysicalQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    MapTag current = GetFrontOfPhysicalQueue();
    while (position > 3 && current.mIndex != kNoThing) {
        position = position - 4;
        current = GetQCellBehind(current);
    }
    cell->mIndex = current.mIndex;
    const uint8_t along = PositionByte(position);
    const uint32_t random = gWorld->GetRand();
    const uint8_t across = static_cast<uint8_t>(random % 0x1c + 0x72);
    if (along <= 0x80) {
        const uint8_t direction = CellAt(gWorldMap, current.mIndex)->GetDirection();
        if (direction == 0x10) {
            *x = across;
            *y = static_cast<uint8_t>(0xff - along);
        } else if (direction == 4) {
            *x = static_cast<uint8_t>(0xff - along);
            *y = across;
        } else if (direction == 1) {
            *x = across;
            *y = along;
        } else if (direction == 0x40) {
            *x = along;
            *y = across;
        } else {
            GAME_ASSERTMSG(0, "Dodgy cell direction\n");
        }
        return 0;
    }
    uint8_t direction = 0;
    const MapTag behind = GetQCellBehind(current);
    if (behind.mIndex == kNoThing) {
        CMapCell* here = CellAt(gWorldMap, current.mIndex);
        const uint8_t neighbours = here->GetNeighbours();
        bool found = false;
        int32_t opposite = 2;
        uint8_t sides[4] = {kDirections[0], kDirections[1], kDirections[2], kDirections[3]};
        for (int32_t i = 0; i < 4; i++) {
            const uint8_t side = sides[i];
            if ((neighbours & side) != 0 && (here->GetNeighbours() & side) != 0) {
                CMapCell* next = here->GetNeighbouringCell(side);
                if (next->IsPath() != 0) {
                    direction = sides[opposite];
                    found = true;
                    break;
                }
            }
            opposite = (i + 3) & 3;
        }
        if (!found) {
            CTRACE(0xc, "*** No path attached to end of queue! ***\n");
            direction = CellAt(gWorldMap, current.mIndex)->GetDirection();
        }
    } else {
        direction = CellAt(gWorldMap, behind.mIndex)->GetDirection();
    }
    if (direction == 0x10) {
        *x = across;
        *y = static_cast<uint8_t>(0xff - along);
    } else if (direction == 4) {
        *x = static_cast<uint8_t>(along + 0x80);
        *y = across;
    } else if (direction == 1) {
        *x = across;
        *y = along;
    } else if (direction == 0x40) {
        *x = along;
        *y = across;
    }
    return 0;
}

// The back of the queue cell, and the place across it by the entry cell's direction.
int32_t CObject::ConvertVirtualQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    const uint8_t along = PositionByte(position);
    const uint32_t random = gWorld->GetRand();
    const uint8_t across = static_cast<uint8_t>(random % 0x1c + 0x72);
    const MapTag back = GetBackOfPhysicalQueue();
    cell->mIndex = back.mIndex;
    const uint16_t entry = mF32;
    CMap* map = gWorldMap;
    GetBackOfPhysicalQueue();
    const uint8_t direction = CellAt(map, entry)->GetDirection();
    if (direction == 0x10) {
        *x = across;
        *y = static_cast<uint8_t>(0xff - along);
    } else if (direction == 4) {
        *x = static_cast<uint8_t>(0xff - along);
        *y = across;
    } else if (direction == 1) {
        *x = across;
        *y = along;
    } else if (direction == 0x40) {
        *x = along;
        *y = across;
    } else {
        GAME_ASSERTMSG(0, "Dodgy cell direction\n");
    }
    return 0;
}

// Where a guest stands at the entry (which 0) or appears at the exit: the cell's column and row, and the record's offsets
// inside the cell (each asserted to be inside (0, 1)) turned by the ride's rotation, in 255ths.
void CObject::GetStandPoint(int32_t which, MapPos* x, MapPos* y) {
    const uint16_t cell = which == 0 ? mF32 : mF34;
    const RideBalanceSlot slotX = which == 0 ? kRideSlot_EntryCellStandPosX : kRideSlot_ExitCellAppearPosX;
    const RideBalanceSlot slotY = which == 0 ? kRideSlot_EntryCellStandPosY : kRideSlot_ExitCellAppearPosY;
    x->mCell = static_cast<uint8_t>((cell - 1) & 0x7f);
    y->mCell = static_cast<uint8_t>(static_cast<uint32_t>(cell - 1) >> 7);
    GAME_ASSERTMSG(StandSlotInside(this, slotX) ? 1 : 0,
                   which == 0 ? "Dodgy X stand point in SAM file for ride %d\n" : "Dodgy X exit point in SAM file for ride %d\n",
                   static_cast<int32_t>(mObjectType));
    GAME_ASSERTMSG(StandSlotInside(this, slotY) ? 1 : 0,
                   which == 0 ? "Dodgy Y stand point in SAM file for ride %d\n" : "Dodgy Y exit point in SAM file for ride %d\n",
                   static_cast<int32_t>(mObjectType));
    const float fx = RecordOfType(this)->Value(slotX).mFloat;
    const float fy = RecordOfType(this)->Value(slotY).mFloat;
    // Another rotation leaves the Mac's f30 and f31 as its caller had them; the host uses 0.
    float ox = 0.0f;
    float oy = 0.0f;
    switch (mRotation) {
        case 0: ox = fx; oy = fy; break;
        case 90: ox = fy; oy = 1.0f - fx; break;
        case 180: ox = 1.0f - fx; oy = 1.0f - fy; break;
        case 270: ox = 1.0f - fy; oy = fx; break;
        default: break;
    }
    x->mOffset = static_cast<uint8_t>(ppc_fctiwz(255.0 * static_cast<double>(ox)));
    y->mOffset = static_cast<uint8_t>(ppc_fctiwz(255.0 * static_cast<double>(oy)));
}

// 0x1010a93c (200 bytes). A ride that is open tells the message centre (built first if the Mac has none; the allocation may fail and the Mac posts through
// the NULL then) and closes.
void CObject::NotifyExitBlocked() {
    if (IsClosed()) return;
    CMsgBlockedExit message(mBase.GetThingId());
    MessageCentreForPost()->PostMessage(message);
    CloseObject();
}

CMsgBlockedExit::~CMsgBlockedExit() {}

// 0x1010aaac (192 bytes). Each question is a call, in this order, and the first bad answer stops the walk. A coaster also needs its track to be usable.
int32_t CObject::IsOpenable() {
    if (IsBrokenDown()) return 0;
    if (IsCondemned() != 0) return 0;
    if (IsFlaggedForMaintenance() != 0) return 0;
    if (RequiresUpgrade() != 0) return 0;
    if (IsBOQConnected() == 0) return 0;
    if (IsCoaster() != 0) {
        if (IsCoasterUseable(this) == 0) return 0;
    }
    return 1;
}

// 0x1010ab98 (8 bytes).
int32_t CObject::IsCloseable() { return 1; }

// 0x1010abcc (228 bytes). Says so, stops the ride, says who was being loaded and forgets him, sets the script's variable 6 and puts the hoardings on.
void CObject::CloseObject() {
    CTRACE(kTrace, kClosing, ThingId(*this));
    mF64 = 0;
    const uint16_t loaded = mPersonLoaded;
    CTRACE(kTrace, kPersonLoaded, ThingId(*this), static_cast<int32_t>(loaded));
    mPersonLoaded = 0;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 6, 1);
    ActivateHoardings(gRideAnimSets[mF1C], 1);
}

// 0x1010acdc (344 bytes). The Mac writes IsOpenable's calls out again (the same questions in the same order); when the ride is not openable it says so
// five times and opens it all the same. Then mF64 to 1, the hoardings off, the script's variable 6 to 0 and the state back to normal.
void CObject::Reopen() {
    if (IsOpenable() == 0) {
        for (int32_t i = 0; i < 5; ++i) CTRACE(kTrace, kNonOpenable);
    }
    mF64 = 1;
    DeActivateHoardings(gRideAnimSets[mF1C]);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 6, 0);
    SetState(kStateNormal);
}

// CObject, main app, unit w199 (the Mac files them under Object.cpp, 0x1010ae5c to 0x1010d9cc in pieces): the ride's wear, breakdown, maintenance, repair,
// upgrade and cleaning. GetProspectiveDamagePerTurn (0x1010ae5c, 816 bytes) and GetProspectiveReliability (0x1010b1cc, 60) estimate the wear a turn does;
// AddWearAndTear (0x1010b244, 840) applies it every 64th turn; RepairFully (0x1010b5bc, 1,940) repairs the ride and, when it was waiting for an upgrade,
// upgrades it a level; CleanFully (0x1010bda8, 228) cleans it; RequestMaintenance (0x1010beb8, 808) flags it for maintenance (closing it) or takes the flag
// off (opening it); the staff member the ride is assigned to (AssignStaffMember 0x1010c4d4, UnassignStaffMember 0x1010c554, GetAssignedStaffMember
// 0x1010c5a0, IsBeingWorkedUpon 0x1010c6d8); the one-line state tests IsCondemned (0x1010c8bc), IsDirty (0x1010c900), IsFlaggedForMaintenance (0x1010c980),
// RequiresUpgrade (0x1010c9c0), StartFixing (0x1010ca08), IsPlayingFixAnim (0x1010ca6c); RequestBreakdown (0x1010d90c) and SetWornFlag (0x1010d994).
// The other CObject members are in Object.cpp, Object_turn.cpp, Object_queries.cpp and Object_queue.cpp.

namespace {
constexpr const char* kRepairing = "Object %d: repairing fully\n";                                         // +0x734
constexpr const char* kLevelThree = "Trying to upgrade a ride already at level three\n";                   // +0x750
constexpr const char* kCancelled = "Object %d: has cancelled its request for maintenance\n";               // +0x79f
constexpr const char* kTakenTooLong = "Ride: removing staff member from this ride, he's taken too long to fix it\n";   // +0x855
constexpr const char* kRequestedBreakdown = "Object %d: requested breakdown\n";                            // +0xb45

// The data block RepairFully's upgrade effect spawns its sprite script with (0x1029fd30, 0x18 bytes of the sprite engine's data, its last word is its own
// Mac address; a twin sits at 0x1029fd18). The words are pointers into the TOC area and small numbers, meaning unknown. The host gives it an opaque block
// of the same words under an invented name.
uint32_t gUpgradeSpawnBlock[6] = {0x10266a50, 0, 0x10266a60, 0, 0x10266958, 0x1029fd30};

// A slot of the record: the first upgrade level's slot moved up by the ride's level (each level is 16 slots, 0x40 bytes of the Mac record).
int32_t UpgradeValue(const RideInfo* info, RideBalanceSlot slot, uint8_t level) {
    return info->Value(static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16)).mInteger;
}

// The ride may be opened: it is not broken down, condemned, flagged for maintenance or waiting for an upgrade, its queue is connected, and a coaster has
// a usable track. The calls are made in this order and stop at the first failure.
bool CanOpen(CObject* ride) {
    if (ride->IsBrokenDown() || ride->IsCondemned() != 0 || ride->IsFlaggedForMaintenance() != 0 || ride->RequiresUpgrade() != 0 ||
        ride->IsBOQConnected() == 0) {
        return false;
    }
    return ride->IsCoaster() == 0 || IsCoasterUseable(ride) != 0;
}

// Opens the ride: the ride is not told when it cannot be opened (five level 2 traces say so, with no number for their %d), mF64 goes to 1, the hoardings
// go off, the script's variable 6 to 0 and the ride's state to normal. RepairFully and RequestMaintenance both open the ride this way.
void OpenRide(CObject* ride, bool openable) {
    if (!openable) {
        for (int32_t i = 0; i < 5; ++i) CTRACE(2, kNonOpenable);
    }
    ride->mF64 = 1;
    DeActivateHoardings(gRideAnimSets[ride->mF1C]);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(ride->mF20)), 6, 0);
    ride->SetState(CObject::kStateNormal);
}

// Closes the ride: says so, mF64 to 0, says who was being loaded and forgets him, the script's variable 6 to 1 and the hoardings on.
void CloseRide(CObject* ride) {
    const uint16_t closing = ride->mBase.GetThingId().mIndex;
    CTRACE(2, kClosing, static_cast<int32_t>(closing));
    ride->mF64 = 0;
    const uint16_t loaded = ride->mPersonLoaded;
    const uint16_t id = ride->mBase.GetThingId().mIndex;
    CTRACE(2, kPersonLoaded, static_cast<int32_t>(id), static_cast<int32_t>(loaded));
    ride->mPersonLoaded = 0;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(ride->mF20)), 6, 1);
    ActivateHoardings(gRideAnimSets[ride->mF1C], 1);
}
}  // namespace

// The estimate of the damage a turn does to the ride, with the ride's operating speed, the number of people it takes and the duration (the third is never
// read). Two halves, each in 0.1 to 1.1 times a ratio: the speed over the record's maximum speed, and the people over the record's maximum capacity (the
// capacity argument when `useCapacity`, else the script's variable 5, the people on the ride, clamped to 0 up to the maximum); a half is 0.1 plus its ratio
// when below its upgrade level's red line (the speed, and the people divided by 10), else half of 1.1 plus its ratio. The answer is the mean of the halves
// times the level's wear rate. All in single precision, the conversions from words signed.
double CObject::GetProspectiveDamagePerTurn(float speed, float capacity, float duration, bool useCapacity) const {
    (void)duration;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    const int32_t onRide = RSSE_Getvar(script, 5);
    int32_t people = ppc_fctiwz(static_cast<double>(static_cast<float>(onRide)));
    if (people < 0) {
        people = 0;
    } else if (people > InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger) {
        people = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
    }
    const float maxSpeed = static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxSpeed).mInteger);
    const float speedRatio = 0.9f * (speed / maxSpeed);
    const float redLineSpeed = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_RedLineSpeed, mF4C));
    float speedHalf;
    if (speed < redLineSpeed) {
        speedHalf = 0.1f + speedRatio;
    } else {
        speedHalf = (1.1f + speedRatio) * 0.5f;
    }
    float peopleRatio;
    if (useCapacity) {
        peopleRatio = 0.9f * (capacity / static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger));
    } else {
        peopleRatio = 0.9f * (static_cast<float>(people) / static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger));
    }
    const float load = static_cast<float>(people) / 10.0f;
    const float redLineCapacity = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_RedLineCapacity, mF4C));
    float peopleHalf;
    if (load < redLineCapacity) {
        peopleHalf = 0.1f + peopleRatio;
    } else {
        peopleHalf = (1.1f + peopleRatio) * 0.5f;
    }
    const float mean = (speedHalf + peopleHalf) * 0.5f;
    const float wearRate = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_WearRate, mF4C));
    return static_cast<double>(mean * wearRate);
}

// 100 minus nine times the estimated damage a turn (as a word, truncated), the people taken from the capacity argument.
int32_t CObject::GetProspectiveReliability(float speed, float capacity, float duration) const {
    const double damage = GetProspectiveDamagePerTurn(speed, capacity, duration, true);
    const float scaled = 9.0f * static_cast<float>(damage);
    return 100 - ppc_fctiwz(static_cast<double>(scaled));
}

// Every 64th world turn, for a ride that wears (its level's wear rate is not 0, and the online game type wears nothing) and runs (or runs continuously,
// mFlags 0x100): takes the estimated damage of the ride's script values (0 while the script's variable 5, the people on the ride, is 0) off the condition
// (mF40, clamped to 0 to 100) and a fiftieth of it off the remaining life (mF44). A condition below 20 sets the script's variable 8 (the worn flag); below
// 10 and the damage would reach 10 the advisor is asked (message 0x42, about this object type). A NaN damage takes the plain subtraction, as in the original.
void CObject::AddWearAndTear() {
    uint32_t wearRate = static_cast<uint32_t>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_WearRate, mF4C));
    if (TheGameType().mType == kGameTypeOnline) wearRate = 0;
    if (wearRate == 0) return;
    if (IsRunning() == 0 && (mFlags & 0x100) == 0) return;
    if ((gWorld->mTurn & 0x3f) != 0) return;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    float damage;
    if (RSSE_Getvar(script, 5) == 0) {
        damage = 0.0f;
    } else {
        RSSE_SCRIPT* again = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        const float speed = static_cast<float>(RSSE_GetRideSpeed(again));
        const float capacity = static_cast<float>(RSSE_Getvar(again, 2));
        const float duration = static_cast<float>(RSSE_Getvar(again, 3));
        damage = static_cast<float>(GetProspectiveDamagePerTurn(speed, capacity, duration, false));
    }
    const float lowered = -damage + mF40;
    if (!(lowered > 100.0f)) {
        if (!(lowered < 0.0f)) {
            mF40 = mF40 - damage;
        } else {
            mF40 = 0.0f;
        }
    } else {
        mF40 = 100.0f;
    }
    const float wear = 0.02f * damage;
    const float aged = -wear + mF44;
    if (!(aged > 100.0f)) {
        if (!(aged < 0.0f)) {
            mF44 = mF44 - wear;
        } else {
            mF44 = 0.0f;
        }
    } else {
        mF44 = 100.0f;
    }
    if (static_cast<float>(LowByte(mF40)) < 20.0f) RSSE_Setvar(script, 8, 1);
    if (static_cast<float>(LowByte(mF40)) < 10.0f) {
        if (damage + static_cast<float>(LowByte(mF40)) >= 10.0f) {
            const uint16_t type = mObjectType;
            CWorld* world = gWorld;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = mBase.GetThingId();
            const CAdvisorRequest request(static_cast<AdvisorMessage>(0x42), id, type, -1, false, -1);
            world->GetAdvisor()->QueueRequest(request);
        }
    }
}

// Repairs the ride fully (the staff member is not read): the condition to 100, the worn flag (script variable 8) off, the staff member and the maintenance
// flag off. A ride that waits for an upgrade (state 2) goes up a level (asserting it was below level three), takes its upgrade effect's old sprite script
// away, shows the effect (RSSE_UpgradeRideEffect, and a new sprite script at level 1 and 2 over the front of the physical queue; the rotations other than
// 0, 90, 180 and 270 leave the position in registers the original never set), pays the level's cost of upgrade and takes the level's speed, capacity and
// duration (each only when above 0; the capacity and the duration are clamped to the record's minimum and maximum, the capacity only when they add up
// above 0, and cut to a byte). Any other ride sets the script's variable 4 to 0 and takes its hoardings off. Last, in all cases, the ride is opened (see
// OpenRide), and a coaster without a usable track is closed again (see CloseRide).
void CObject::RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) {
    (void)staff;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    CTRACE(0xc, kRepairing);
    mF40 = 100.0f;
    RSSE_Setvar(script, 8, 0);
    mF5A = 0;
    mF60 = 0;
    if (mState == kStateWantsUpgrade) {
        GAME_ASSERTMSG(static_cast<int32_t>((static_cast<uint32_t>(mF4C) - 3u) >> 31), kLevelThree);
        mF4C = static_cast<uint8_t>(mF4C + 1);
        if (mF50 != 0) SCR_KillScript(static_cast<int32_t>(mF50));
        // The original tests mFlags & 8 before each of the next two calls and makes the same call on both sides.
        const uint16_t frontX = GetFrontOfPhysicalQueue().mIndex;
        const float cellX = 0.5f + static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(frontX) - 1u) & 0x7f));
        const uint16_t frontZ = GetFrontOfPhysicalQueue().mIndex;
        const float cellZ = 0.5f + static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(frontZ) - 1u) >> 7));
        float x = 0.0f;
        float z = 0.0f;
        switch (mRotation) {
            case 0:
            case 0x5a:
            case 0xb4:
            case 0x10e:
                x = 10.0f * cellX;
                z = 10.0f * cellZ;
                break;
            default:
                break;   // the original leaves two callee-saved registers as its caller had them: 0 stands in
        }
        RSSE_UpgradeRideEffect(static_cast<int32_t>(mF20));
        if (mF4C == 1 || mF4C == 2) {
            mF50 = static_cast<uint32_t>(ESP_SpawnScript(gUpgradeSpawnBlock, static_cast<ESP_Sprites>(0xc), 0, static_cast<ACTS>(0), x, 0.0f, z));
        }
        RideInfo* info = InfoOf(*this);
        gWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_CostOfUpgrade, mF4C)));
        const int32_t speed = UpgradeValue(info, kRideSlot_Upgrades_InitSpeed, mF4C);
        if (speed > 0) {
            RSSE_SetRideSpeed(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), speed);
            CTRACE(0x7c, kSpeed, speed);
            mF54 = static_cast<uint32_t>(speed);
        }
        uint32_t capacity = static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_InitCapacity, mF4C));
        if (static_cast<int32_t>(capacity) > 0) {
            capacity &= 0xff;
            const int32_t maxCapacity = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
            const int32_t minCapacity = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
            if (minCapacity + maxCapacity > 0) {
                if (static_cast<int32_t>(capacity) < InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger) {
                    capacity = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger);
                } else if (InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger < static_cast<int32_t>(capacity)) {
                    capacity = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger);
                }
                capacity &= 0xff;
            }
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 2, static_cast<int32_t>(capacity));
            CTRACE(0x7c, kCapacity, static_cast<int32_t>(capacity));
            mF59 = static_cast<uint8_t>(capacity);
        }
        uint32_t duration = static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_InitDuration, mF4C));
        if (static_cast<int32_t>(duration) > 0) {
            duration &= 0xff;
            if (static_cast<int32_t>(duration) < InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger) {
                duration = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger);
            } else if (InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger < static_cast<int32_t>(duration)) {
                duration = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger);
            }
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 3, static_cast<int32_t>(duration & 0xff));
            CTRACE(0x7c, kDuration, static_cast<int32_t>(duration & 0xff));
            mF58 = static_cast<uint8_t>(duration);
        }
    } else {
        RSSE_Getvar(script, 7);   // both reads are dead in the original, and so is the script they read
        RSSE_Getvar(script, 4);
        RSSE_Setvar(script, 4, 0);
        DeActivateHoardings(gRideAnimSets[mF1C]);
    }
    OpenRide(this, CanOpen(this));
    if (IsCoaster() != 0 && IsCoasterUseable(this) == 0) CloseRide(this);
}

// Cleans the ride: a dirty one (see IsDirty) takes the litter effect (kind 6) out of its map cell and relief (kind 1) in; the worn flag (script variable 8)
// goes off, the condition to 100, and the staff member and the maintenance flag off. The cell is the map cell of the ride's offset (1 based id).
void CObject::CleanFully() {
    if (IsDirty() != 0) {
        const uint16_t cell = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
        gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(6), tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});   // 6 has no name (a dirt effector)
        const uint16_t again = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
        gWorldMap->AddRegionalEffector(kRegionEffectorRelief, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{again});
    }
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 0);
    mF40 = 100.0f;
    mF5A = 0;
    mF60 = 0;
}

// Flags the ride for maintenance (`on` not 0, when it is not flagged yet: closes it, see CloseRide) or takes the flag off (`on` 0, when it is flagged: opens
// the ride, see OpenRide, when it can be opened). Always answers 1.
int32_t CObject::RequestMaintenance(int32_t on) {
    if (on == 0) {
        const uint16_t id = mBase.GetThingId().mIndex;
        CTRACE(0x7c, kCancelled, static_cast<int32_t>(id));
        if (mF60 != 0) {
            mF60 = 0;
            if (CanOpen(this)) OpenRide(this, CanOpen(this));
        }
    } else if (mF60 == 0) {
        const uint16_t id = mBase.GetThingId().mIndex;
        CTRACE(0x7c, kWantsMaintenance, static_cast<int32_t>(id));
        mF60 = 1;
        CloseRide(this);
    }
    return 1;
}

// 0x1010c214 (656 bytes). Asking (on is not 0): a choosable ride wants an upgrade (the state and the hoardings, flag 8); any other says it cannot (the
// first trace line is the Mac's maintenance text). Withdrawing: a ride that wanted one is opened again (Reopen, after the hoardings are taken off; the
// Mac writes Reopen out inline), any other says it never asked. Withdrawing always answers 1.
int32_t CObject::RequestUpgrade(int32_t on) {
    if (on != 0) {
        CTRACE(kSettingTrace, kWantsMaintenance, ThingId(*this));
        if ((mFlags & 4) != 0) {
            SetState(kStateWantsUpgrade);
            ActivateHoardings(gRideAnimSets[mF1C], 8);
            return 1;
        }
        CTRACE(kTrace, kCannotUpgrade, ThingId(*this));
        return 0;
    }
    if (RequiresUpgrade() != 0) {
        CTRACE(kTrace, kCancelling, ThingId(*this));
        DeActivateHoardings(gRideAnimSets[mF1C]);
        Reopen();
    } else {
        CTRACE(kTrace, kNeverRequested, ThingId(*this));
    }
    return 1;
}

// The staff member assigned to the ride, and the turn it was assigned on.
void CObject::AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) {
    mF5A = staff.mIndex;
    mF5C = gWorld->mTurn;
}

void CObject::UnassignStaffMember() {
    mF5A = 0;
    mF5C = 0;
}

// The assigned staff member (nobody is 0). One assigned more than 100 turns ago (the turn counter unsigned, the sum wrapping) who is not allocated to this
// ride any more is taken off, with a trace, and the answer is nobody.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    if (mF5A == kNoThing) return {kNoThing};
    if (static_cast<uint32_t>(mF5C + 100u) < gWorld->mTurn) {
        const uint16_t staffId = mF5A;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
        if (staff->IsAllocatedToRide(mBase.GetThingId()) == 0) {
            CTRACE(0xc, kTakenTooLong);
            mF5A = 0;
            mF5C = 0;
            return {kNoThing};
        }
    }
    return {mF5A};
}

// Whether the assigned staff member (the same timeout as GetAssignedStaffMember, inlined) works on this ride: nobody is 0, else the staff member's answer.
int32_t CObject::IsBeingWorkedUpon() {
    uint16_t assigned;
    if (mF5A == kNoThing) {
        assigned = kNoThing;
    } else {
        assigned = mF5A;
        if (static_cast<uint32_t>(mF5C + 100u) < gWorld->mTurn) {
            const uint16_t staffId = mF5A;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
            if (staff->IsAllocatedToRide(mBase.GetThingId()) == 0) {
                CTRACE(0xc, kTakenTooLong);
                mF5A = 0;
                mF5C = 0;
                assigned = kNoThing;
            }
        }
    }
    if (assigned == kNoThing) return 0;
    const uint16_t staffId = mF5A;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
    return staff->IsWorkingOnAllocatedObject(mBase.GetThingId());
}

// Two one-line questions about a ride or shop object (main app, Object.cpp range; unit w151): CObject::IsClosed 0x1010cae8 and
// CObject::IsBrokenDown 0x1010c878 (24 bytes each). Both answer a 0 or 1 byte in r3 (cntlzw of the difference, shifted).

// The object is broken down when its state word (+0x198) is 1 (kStateBrokenDown).
bool CObject::IsBrokenDown() const { return mState == kStateBrokenDown; }

int32_t CObject::IsCondemned() const {
    return mState == kStateCondemned ? 1 : 0;
}

// A relief ride (mFlags 1) whose condition (the low byte of the float as fctiwz converts it) is below 25.
int32_t CObject::IsDirty() const {
    if ((mFlags & 1) != 0 && static_cast<float>(LowByte(mF40)) < 25.0f) return 1;
    return 0;
}

int32_t CObject::IsFlaggedForMaintenance() const {
    return static_cast<int32_t>(mF60);
}

int32_t CObject::RequiresUpgrade() const {
    return mState == kStateWantsUpgrade ? 1 : 0;
}

// The ride's script variable 4 is the breakdown request (RequestBreakdown sets it to 1, StartFixing to 0), 7 is 1 while the fixing animation plays and 8 is the
// worn flag (SetWornFlag).
void CObject::StartFixing() {
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 4, 0);
}

int32_t CObject::IsPlayingFixAnim() {
    return RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 7) > 0 ? 1 : 0;
}

// The object is closed when its running word (+0x64, "1 while running": SetState clears it) is 0.
bool CObject::IsClosed() const { return mF64 == 0; }

// 0x1010cb28 (76 bytes). Only a positive variable counts (the Mac builds it with the carry of a subtraction).
int32_t CObject::IsRunning() const {
    return RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 9) > 0 ? 1 : 0;
}

// The excitement a ride would have. A sideshow (a, b, c the prize value, the price and the chance of winning): 20 plus
// 0.8 of a tenth of c times the root of the prize's margin (held to 0..100), as a byte. A coaster: 50 plus half its
// second peep rating. Any other ride: the record's level (for a track ride 60 percent of it plus the weighted track
// sections, held to 0..40, the sum held to 0..100) times the speed and duration ratios to the level's initial ones, each
// held to [0.75, 1.25].
int32_t CObject::GetProspectiveExcitementLevel(int32_t a, int32_t b, int32_t c) const {
    if (RecordOfType(this)->mUIType == 2) {
        int32_t margin = a - b;
        if (margin < 0) margin = 0;
        else if (margin > 100) margin = 100;
        const double root = std::sqrt(static_cast<double>(margin));
        const float scaled = static_cast<float>((static_cast<double>(c) * root) / 10.0);
        const uint8_t level = static_cast<uint8_t>(ppc_fctiwz(0.8f * scaled));
        return static_cast<uint8_t>(level + 0x14);
    }
    if (IsCoaster() != 0) {
        const uint32_t handle = RSSE_GetCoasterHandle(static_cast<int32_t>(mF20));
        if (handle == 0) return 0;
        float first;
        float second;
        GetCoasterPeepStats(handle, &first, &second);
        return ppc_fctiwz(std::fmaf(second, 0.5f, 50.0f));
    }
    int32_t base = RecordOfType(this)->Value(kRideSlot_ExcitementLevel).mInteger;
    const int32_t speed = LevelSlot(RecordOfType(this), kRideSlot_Upgrades_InitSpeed, mF4C);
    const int32_t duration = LevelSlot(RecordOfType(this), kRideSlot_Upgrades_InitDuration, mF4C);
    if (mF24 != 0) {
        int32_t p = 0;
        int32_t q = 0;
        int32_t r = 0;
        int32_t s = 0;
        BMP_CountTrackSections(mF24, &p, &q, &r, &s);
        int32_t track = static_cast<int32_t>(static_cast<uint32_t>(r) * 3u + static_cast<uint32_t>(p) * 2u + static_cast<uint32_t>(q));
        if (track < 0) track = 0;
        else if (track > 40) track = 40;
        base = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(static_cast<uint32_t>(base) * 60u) / 100) +
                                    static_cast<uint32_t>(track));
        if (base < 0) base = 0;
        else if (base > 100) base = 100;
    }
    // Four more fetches whose answers are not used.
    RecordOfType(this);
    RecordOfType(this);
    RecordOfType(this);
    RecordOfType(this);
    const float speedRatio = ClampRatio(static_cast<float>(a) / static_cast<float>(speed));
    const float durationRatio = ClampRatio(static_cast<float>(b) / static_cast<float>(duration));
    const float ratio = speedRatio * durationRatio;
    return ppc_fctiwz(static_cast<float>(base) * ratio);
}

// The ride's excitement: with the flag the record's level as it is; else 0 for a level of 0, the sideshow's figure from its
// prize, price and chance of winning, or a ride's from its speed, duration and capacity (the word goes back as it is).
uint8_t CObject::GetExcitementLevel(bool flag) const {
    const int32_t level = RecordOfType(this)->Value(kRideSlot_ExcitementLevel).mInteger;
    if (flag) return static_cast<uint8_t>(level);
    if (level == 0) return 0;
    if (RecordOfType(this)->mUIType == 2) {
        const uint8_t chance = GetChanceOfWinning();
        const int32_t price = static_cast<int32_t>(GetPricePerUse());
        const int32_t prize = static_cast<int32_t>(GetPrizeValue());
        return static_cast<uint8_t>(GetProspectiveExcitementLevel(prize, price, chance));
    }
    return static_cast<uint8_t>(GetProspectiveExcitementLevel(static_cast<int32_t>(mF54), mF58, mF59));
}

// 0x1010d0e0 (548 bytes). A ride that is broken down, condemned or closed refuses (a trace of level 0xc). Otherwise the person should be the one being
// loaded (a trace and an assert say so when he is not), the ride forgets the person being loaded and gives the script's variable 0 to the person, when
// the script holds nobody; else it refuses with a trace of level 0xc.
int32_t CObject::AdmitPerson(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    const int32_t state = mState;
    if (state == kStateBrokenDown || mF64 == 0 || state == kStateCondemned) {
        const uint16_t who = person.mIndex;
        const uint16_t type = mObjectType;
        const ThingId_2 id = mBase.GetThingId();
        CTRACE(0xc, kCannotAdmit, id.mIndex, type, who);
        return 0;
    }
    if (mPersonLoaded != person.mIndex) {
        const uint32_t turn = gWorld->mTurn;
        const uint16_t who = person.mIndex;
        const uint16_t loaded = mPersonLoaded;
        const uint16_t type = mObjectType;
        const ThingId_2 id = mBase.GetThingId();
        CTRACE(2, kLoadedIs, id.mIndex, type, loaded, who, turn);
        GAME_ASSERTMSG(0, kWrongPerson);
    }
    const uint16_t loaded = mPersonLoaded;
    const ThingId_2 id = mBase.GetThingId();
    CTRACE(2, kAdmitting, id.mIndex, loaded);
    mPersonLoaded = 0;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (RSSE_Getvar(script, 0) == 0) {
        RSSE_Setvar(script, 0, person.mIndex);
        return 1;
    }
    const uint32_t turn = gWorld->mTurn;
    const uint16_t who = person.mIndex;
    const uint16_t type = mObjectType;
    const ThingId_2 again = mBase.GetThingId();
    CTRACE(0xc, kChangedMind, again.mIndex, type, who, turn);
    return 0;
}

// 0x1010d35c (96 bytes). Admitting is finished when the script's variable 0 is no longer the head of the queue.
int32_t CObject::FinishedAdmitting() {
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    return static_cast<uint32_t>(RSSE_Getvar(script, 0)) != mQueuedPerson;
}

// 0x1010d3ec (32 bytes). The Mac answers a byte and calls nothing.
bool CObject::ReadyToAdmit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    return mPersonLoaded == person.mIndex;
}

// 0x1010d464 (360 bytes). The person should be the one being loaded (an assert says so when he is not). The ride says so, forgets the person being loaded
// and, when the script's variable 0 still holds the person, says so again and clears the variable.
void CObject::PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    const uint16_t loaded = mPersonLoaded;
    const ThingId_2 id = mBase.GetThingId();
    GAME_ASSERTMSG(mPersonLoaded == person.mIndex, kNotExpecting, person.mIndex, id.mIndex, loaded);
    const ThingId_2 again = mBase.GetThingId();
    CTRACE(2, kLeftAfter, again.mIndex, person.mIndex);
    mPersonLoaded = 0;
    const uint16_t who = person.mIndex;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (static_cast<uint32_t>(RSSE_Getvar(script, 0)) != who) return;
    const ThingId_2 last = mBase.GetThingId();
    CTRACE(2, kLetMeOn, last.mIndex, who);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 0);
}

// Three jobs while the ride's state is not 3 or 4 (condemned rides do none of them, the fireworks apart):
// 1. A fireworks ride whose script variable 0 is 1 takes its map effect out of the region and sets the variable to 2.
// 2. Every eighth turn (the world's turn count), while the ride runs (mF64) and is not broken down: wears the ride out, and when the wear
//    shows (the low byte of the condition or of the ride quality is 0), it is not the relief ride and the script's variable 4 is 0: a
//    condition of 0 breaks the ride, else the condition goes down by 5 (clamped to 0 to 100; a NaN takes the plain subtraction) and the
//    ride breaks.
// 3. A ride with flag 1 whose quality's low byte is below 25 is marked worn.
// Last, a queued person who is gone or not queueing any more is forgotten.
void CObject::UpdateTurn() {
    if ((mFlags & 0x80) != 0) {
        RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        if (RSSE_Getvar(script, 0) == 1) {
            const uint16_t cell = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
            gWorldMap->RemoveRegionalEffector(kRegionEffectorFireworks, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 2);
        }
    }
    if (mState == 3 || mState == 4) return;
    if (mState != 1 && mF64 != 0 && (gWorld->mTurn & 7) == 0) {
        RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        AddWearAndTear();
        if ((LowByte(mF44) == 0 || LowByte(mF40) == 0) && (mFlags & 1) == 0 && RSSE_Getvar(script, 4) == 0) {
            const float condition = mF44;
            if (LowByte(condition) == 0) {
                RequestBreakdown();
            } else {
                const float lowered = -5.0f + condition;
                if (!(lowered > 100.0f)) {
                    if (!(lowered < 0.0f)) {
                        mF44 = mF44 + -5.0f;
                    } else {
                        mF44 = 0.0f;
                    }
                } else {
                    mF44 = 100.0f;
                }
                RequestBreakdown();
            }
        }
    }
    if ((mFlags & 1) != 0) {
        const float quality = static_cast<float>(LowByte(mF40));
        if (quality < 25.0f) SetWornFlag();
    }
    if (mQueuedPerson != kNoThing) {
        const uint16_t queued = mQueuedPerson;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[queued].mThing);
        if (person == nullptr || person->InQueueingState() == 0) mQueuedPerson = 0;
    }
}

void CObject::RequestBreakdown() {
    CTRACE(2, kRequestedBreakdown);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 4, 1);
}

void CObject::SetWornFlag() {
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 1);
}

// 0x1010d9f8 (16 bytes) and 0x1010da64 (12 bytes). The id goes by value in the HIGH halfword of a register; the answer through a hidden pointer.
void CObject::SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mF2C = id.mIndex;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    return {mF2C};
}

// State 0 runs the ride's normal handler; 1, 2 and 4 (broken down, wants an upgrade, condemned) look at the script's variable 0: when it is not
// the queued person and that person (a thing in state 0xe) has not noticed he was admitted, he is forced onto the ride; else a queued person is
// taken out of the queue and dismissed. Either way the ride then lets its people off. State 3 does nothing; any other state asserts.
void CObject::ModelState() {
    if (mState == 3) return;
    if (mState == 0) {
        ProcessStateFunctioning();
        return;
    }
    if (mState < 0 || mState >= 5) {
        GAME_ASSERTMSG(0, "Unknown state in CObject::ModelState\n");
        return;
    }
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    const uint32_t variable = static_cast<uint32_t>(RSSE_Getvar(script, 0));
    const uint16_t queued = mQueuedPerson;
    bool dismiss = true;
    if (variable != queued) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[queued].mThing);
        if (WordAt_2(person, 0x220) == 0xe) {
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> personId = reinterpret_cast<CThing*>(person)->GetThingId();
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> rideId = mBase.GetThingId();
            CTRACE(2, "Object %d: script admitted person %d but he doesn't know yet, forcing him onto ride\n", rideId.mIndex, personId.mIndex);
            ObjectForceRideAdmittance(person);
            dismiss = false;
        }
    }
    if (dismiss && mQueuedPerson != kNoThing) {
        const uint16_t again = mQueuedPerson;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[again].mThing);
        RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{mQueuedPerson});
        ObjectDismissFromQueue(person);
    }
    LetPeopleOff();
}

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

CMsgRideWantsUpgrade::~CMsgRideWantsUpgrade() {}
CMsgRideBrokenDown::~CMsgRideBrokenDown() {}
CMsgRideCondemned::~CMsgRideCondemned() {}

// 0x1010e61c (952 bytes). A closed ride (mF64 0): when the script's variable 0 is not the head of the queue and the head is admitted but unaware (state
// 0xe), it forces him onto the ride and is done; otherwise it takes the head out of the queue and dismisses him. An open ride: the capacity must be
// above 0; when the script holds nobody and MayLoadNext agrees, it loads the head of the queue (ReadyForAdmitting and OnAdmittance) and makes him the
// person being loaded; in every other case it checks that the person being loaded is in the state of being loaded (0xd) and forgets him when he is not.
void CObject::LetPeopleOn() {
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (mF64 == 0) {
        const uint32_t held = static_cast<uint32_t>(RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0));
        const uint16_t queued = mQueuedPerson;
        if (held != queued) {
            CPerson* guest = GuestOf(queued);
            if (WordAt_2(guest, kGuestState) == kAdmittedUnaware) {
                const ThingId_2 personId = IdOfGuest(guest);
                const ThingId_2 rideId = mBase.GetThingId();
                CTRACE(2, kForcing, rideId.mIndex, personId.mIndex);
                ObjectForceRideAdmittance(guest);
                return;
            }
        }
        if (mQueuedPerson == kNoThing) return;
        const uint16_t head = mQueuedPerson;
        CPerson* guest = GuestOf(head);
        RemovePersonFromQueue(ThingId_2{mQueuedPerson});
        ObjectDismissFromQueue(guest);
        return;
    }
    GAME_ASSERTMSG(RSSE_Getvar(script, 2) > 0, kZeroCapacity);
    if (RSSE_Getvar(script, 0) == 0 && MayLoadNext(*this, script)) {
        if (mQueuedPerson == kNobodyQueued) return;
        const uint16_t head = mQueuedPerson;
        CPerson* guest = GuestOf(head);
        if (ObjectReadyForAdmitting(guest) == 0) return;
        ObjectOnAdmittance(guest);
        mPersonLoaded = mQueuedPerson;
        const uint16_t loaded = mPersonLoaded;
        const ThingId_2 id = mBase.GetThingId();
        CTRACE(2, kLoading, id.mIndex, loaded);
        return;
    }
    if (mPersonLoaded == kNobodyLoaded) return;
    const uint16_t first = mPersonLoaded;
    CPerson* guest = GuestOf(first);
    if (WordAt_2(guest, kGuestState) == kBeingLoaded) return;
    const uint16_t loaded = mPersonLoaded;
    const ThingId_2 id = mBase.GetThingId();
    GAME_ASSERTMSG(0, kThinksOtherwise, id.mIndex, loaded);
    mPersonLoaded = 0;
}

// 0x1010ea00 (280 bytes).
void CObject::LetPeopleOff() {
    LetGuestOff(*this, RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
}

// 0x1010eb44 (528 bytes). The ride loads its guests, then reads the script's variable 7: 0 lets the guests off (a second call for the script, as the
// Mac makes it); otherwise the ride breaks down while the low byte of mF44 (the remaining life) is not 0 and is condemned when it is, each with the hoardings
// of its anim set (2 and 4) and a trace of level 2.
void CObject::ProcessStateFunctioning() {
    LetPeopleOn();
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (RSSE_Getvar(script, 7) == 0) {
        LetGuestOff(*this, RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
        return;
    }
    if (LowByte(mF44) != 0) {
        const ThingId_2 id = mBase.GetThingId();
        CTRACE(2, kBreakingDown, id.mIndex);
        ActivateHoardings(gRideAnimSets[mF1C], 2);
        SetState(kStateBrokenDown);
        return;
    }
    const ThingId_2 id = mBase.GetThingId();
    CTRACE(2, kCondemning, id.mIndex);
    ActivateHoardings(gRideAnimSets[mF1C], 4);
    SetState(kStateCondemned);
}

// 0x1010ed8c (264 bytes). The ride record's word +0x80 (the first person sprites) goes to the camera in every case. A ride with a camera mesh of its own
// (mF24 not 0 and BMP_GetCameraMesh finds one) uses it and is done; a ride whose script drives a fly tour (RSSE_GetTourBaseID not 0) uses the tour's mesh;
// the others use their anim set's index (mF1C) and the record's word +0x11c (the constrain camera slot). `on` is the third argument in every case.
void CObject::GoFirstPerson(int32_t on) const {
    RideInfo* info = InfoOf(*this);
    const int32_t sprites = info->Value(kRideSlot_FirstPersonSprites).mInteger;
    if (mF24 != 0) {
        const int32_t mesh = BMP_GetCameraMesh(mF24);
        if (static_cast<uint32_t>(mesh) != 0) {
            ToggleRideRideIn1stPerson(mesh, sprites, on, nullptr, 1, 0);
            return;
        }
    }
    const int32_t tour = RSSE_GetTourBaseID(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
    if (tour != 0) {
        ToggleRideRideIn1stPerson(FLY_GetCameraMesh(tour), sprites, on, nullptr, 1, 0);
        return;
    }
    ToggleRideRideIn1stPerson(static_cast<int32_t>(mF1C), sprites, on, nullptr, 1, info->Value(kRideSlot_ConstrainCamera).mInteger);
}

// One customer walked away: the total and the month's running count.
void CObject::WalkedAway() {
    mF1A0 += 1;
    mHistory22C.mTemp += 1;
}

// One customer used the object: the total and the month's running count.
void CObject::HadCustomer() {
    mF19C += 1;
    mHistory1A4.mTemp += 1;
}

// Money came in: deposited, counted in the takings and in the challenge counters of the ride's kind.
void CObject::RecordNewTakings(uint32_t amount) {
    gWorld->GetBankAccount()->MakeDeposit(amount);
    mHistory6C.mTemp += amount;
    mF17C += amount;
    if (RecordOf(this)->mUIType != kUIObjShop) {
        if (RecordOf(this)->mUIType != kUIObjSideshow) return;
        gWorld->GetParkAnalyser()->mHistory1F100[8].mTemp += static_cast<int32_t>(amount);
        AddToCounter(static_cast<ChallengeType>(kChallengeSideshowTakings), static_cast<int32_t>(amount));
        return;
    }
    gWorld->GetParkAnalyser()->mHistory1F100[7].mTemp += static_cast<int32_t>(amount);
    AddToCounter(static_cast<ChallengeType>(kChallengeShopTakings), static_cast<int32_t>(amount));
    // A shop also counts one sale of its kind: the shop type picks the counter, types 2 and 4 look at the special ingredient.
    const int32_t shopType = RecordOf(this)->Value(kRideSlot_ShopType).mInteger;
    int32_t counter = 0;
    switch (shopType) {
        case 1: counter = 5; break;
        case 3: counter = 8; break;
        case 5: counter = 7; break;
        case 6: counter = 6; break;
        case 2:
        case 4:
            switch (RecordOf(this)->Value(kRideSlot_SpecialIngredient).mInteger) {
                case 1: counter = 2; break;
                case 2: counter = 1; break;
                case 3: counter = 3; break;
                case 4: counter = 4; break;
                default: break;
            }
            break;
        default: break;
    }
    if (counter != 0) AddToCounter(static_cast<ChallengeType>(counter), 1);
}

// Money went out: counted in the costs, withdrawn and taken off the challenge counter of the ride's kind.
void CObject::RecordACost(uint32_t amount) {
    mHistoryF4.mTemp += amount;
    mF180 += amount;
    gWorld->GetBankAccount()->MakeWithdrawal(amount);
    if (RecordOf(this)->mUIType == kUIObjShop) {
        AddToCounter(static_cast<ChallengeType>(kChallengeShopTakings), static_cast<int32_t>(0u - amount));
        return;
    }
    if (RecordOf(this)->mUIType == kUIObjSideshow) AddToCounter(static_cast<ChallengeType>(kChallengeSideshowTakings), static_cast<int32_t>(0u - amount));
}

// A sideshow customer won.
void CObject::HadAWinner() { mHistory2B4.mTemp += 1; }

uint32_t CObject::GetPricePerUse() const { return mPricePerUse; }

uint32_t CObject::GetPrizeValue() const { return mCostOfGoods; }

void CObject::SetPrizeValue(int32_t value) { mCostOfGoods = static_cast<uint32_t>(value); }

// The cost of goods for a given quality and ingredient amount.
int32_t CObject::GetProspectiveCostOfGoods(float quality, float ingredient) const { return CostOfGoods(this, quality, ingredient); }

// The cost of goods for the ride's own quality and ingredient amount (the amount is asked first).
int32_t CObject::GetCostOfGoods() const {
    const float ingredient = static_cast<float>(GetAmountOfSpecialIngredient());
    const float quality = static_cast<float>(GetProductQuality());
    return CostOfGoods(this, quality, ingredient);
}

int32_t CObject::GetTotalRevenue() const { return static_cast<int32_t>(mF17C - mF180); }

// The last 30 months' takings minus the last 30 months' costs (the costs are summed first; the doubles are subtracted, then truncated).
int32_t CObject::GetProfitability() const {
    double costs;
    mHistoryF4.GetSumOfLast(costs, kHistoryCount);
    double takings;
    mHistory6C.GetSumOfLast(takings, kHistoryCount);
    return ppc_fctiwz(takings - costs);
}

// The first value is kept; later ones are averaged with the one before (signed, rounded toward zero).
void CObject::ExpressSatisfaction(int32_t value) {
    if (mHistory33C.mTemp == 0) {
        mHistory33C.mTemp = value;
        return;
    }
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(mHistory33C.mTemp) + static_cast<uint32_t>(value));
    mHistory33C.mTemp = sum / 2;
}

int32_t CObject::GetCustomerSatisfaction() const {
    int32_t average;
    mHistory33C.GetAverageOfLast(average, kHistoryCount);
    return average + 50;
}

// Winners as a percentage of the customers over the last 30 months; 0 when no customer was counted. (The Mac converts the customer sum twice
// in a row before the compare; the first conversion is dead, and it sums the customers a second time for the divisor.)
uint32_t CObject::GetPercentageOfWinners() const {
    if (SumOf(mHistory1A4) == 0) return 0;
    const uint32_t winners = SumOf(mHistory2B4);
    const uint32_t customers = SumOf(mHistory1A4);
    return (winners * 100u) / customers;
}

uint8_t CObject::GetProductQuality() const { return static_cast<uint8_t>(mF188); }

void CObject::SetProductQuality(uint8_t quality) { mF188 = quality; }

uint8_t CObject::GetChanceOfWinning() const { return static_cast<uint8_t>(mChanceOfNotLoosing); }

void CObject::SetChanceOfWinning(uint8_t chance) { mChanceOfNotLoosing = chance; }

void CObject::SetAmountOfSpecialIngredient(int32_t amount) { mF194 = static_cast<uint32_t>(amount); }

uint8_t CObject::GetAmountOfSpecialIngredient() const { return static_cast<uint8_t>(mF194); }

// The name of the record's special ingredient (ingredients 1 to 4 are strings 0 to 3 of the table at gText + 0x55c); none gives NULL.
const unsigned char* CObject::GetSpecialIngredientName() const {
    uint32_t id;
    switch (RecordOf(this)->Value(kRideSlot_SpecialIngredient).mInteger) {
        case 1: id = 0; break;
        case 2: id = 1; break;
        case 3: id = 2; break;
        case 4: id = 3; break;
        default: return nullptr;
    }
    return static_cast<const TbITable*>(gText.mResourcePointers[kTextTableIndex])->GetConstString(id);
}

// How much of its price the ride is worth, by its age: the record's scrap value of the first year (the second year, the third, the fourth for
// every other span, a negative one too), at the ride's upgrade level. In the first month with no customer yet a ride is worth all of it.
int32_t CObject::GetResidualValuePercentage() const {
    const int64_t span = BuiltFor(this);
    const int32_t year = static_cast<int32_t>(span / kTicksPerYear);
    if (year == 2) return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear3, mF4C);
    if (year < 2) {
        if (year == 0) {
            const int32_t month = static_cast<int32_t>(span / kTicksPerMonth);
            if (month == 0 && mF19C == 0) {
                if (RecordOf(this)->Value(kRideSlot_Upgrades_ScrapValueYear1).mInteger > 0) return 100;
            }
            return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear1, mF4C);
        }
        if (year >= 0) return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear2, mF4C);
    }
    return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear4, mF4C);
}

// That percentage of the record's cost of the upgrade (element 0), unsigned.
uint32_t CObject::GetCurrentResidualValue() const {
    const RideInfo* record = RecordOf(this);
    const int32_t percentage = GetResidualValuePercentage();
    return static_cast<uint32_t>(static_cast<uint32_t>(percentage) * static_cast<uint32_t>(record->Value(kRideSlot_Upgrades_CostOfUpgrade).mInteger)) / 100u;
}

// A person used the toilet. An online game cleans a dirty one; any other game wears the cleanliness down by amount / 20 (clamped to 0..100) and,
// when that turns the toilet dirty, swaps its map effect.
void CObject::PersonUsedToilet(uint32_t amount) {
    if (TheGameType().mType == kGameTypeOnline) {
        if (!IsDirtyNow(this)) return;
        // What follows is CleanFully (0x1010bda8) inlined: it tests for dirt again.
        if (IsDirtyNow(this)) {
            gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(kRegionEffectorDirty), CellOf(this));
            gWorldMap->AddRegionalEffector(kRegionEffectorRelief, CellOf(this));
        }
        RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 0);
        mF40 = 100.0f;
        mF5A = 0;
        mF60 = 0;
        return;
    }
    const bool wasDirty = IsDirtyNow(this);
    const float wear = static_cast<float>(amount) / 20.0f;
    const float cleaner = -wear + mF40;
    if (cleaner > 100.0f) mF40 = 100.0f;
    else if (cleaner < 0.0f) mF40 = 0.0f;
    else mF40 = mF40 - wear;
    if (IsDirtyNow(this) && !wasDirty) {
        CTRACE(2, "Toilet has become dirty and smelly\n");
        gWorldMap->RemoveRegionalEffector(kRegionEffectorRelief, CellOf(this));
        gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(kRegionEffectorDirty), CellOf(this));
    }
}

// The sideshow's draw: a person wins when the world's random number (modulo 100) is not above the chance. Only sideshows may have a chance below 100.
bool CObject::WillIWin(CPerson*) const {
    const bool allowed = RecordOf(this)->mUIType == kUIObjSideshow || static_cast<uint8_t>(mChanceOfNotLoosing) == 100;
    GAME_ASSERTMSG(allowed, "Non-sideshow object number %d has <100%% chance of winning\n");
    const uint32_t chance = static_cast<uint8_t>(mChanceOfNotLoosing);
    const uint32_t roll = gGameWorld->GetRand() % 100u;
    const bool wins = chance >= roll;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 11, wins ? 1 : 0);
    return wins;
}

// The type the ride upgrades to (its record's UpgradesId).
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    return tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(RecordOfType(this)->Value(kRideSlot_UpgradesId).mInteger)};
}

// The control block of the ride's type.
ObjectControl* CObject::GetControl() {
    return gWorld->GetControlManager()->GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType}, 0);
}

void CObject::SetPricePerUse(int32_t price) { mPricePerUse = static_cast<uint32_t>(price); }

// The record's item name of the ride's type.
const char16_t* CObject::GetFileName() const {
    return gRideDatabase.GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
}

// The ride's full name: both sign lines joined with a blank, or without when the first ends with a hyphen (the hyphen is
// dropped, and the line is cut to 31 characters first); one line alone as it is.
void CObject::GetUIName(char16_t* out, uint32_t size) {
    const uint32_t length = wcslen(mSignA);
    if (length == 0) {
        wcsncpy(out, mSignB, size);
    } else if (mSignA[length - 1] == u'-') {
        gObjectUINameBuffer[0x20] = 0;
        swprintf(gObjectUINameBuffer, 0x20, u"%ls", mSignA);
        gObjectUINameBuffer[length - 1] = 0;
        swprintf(out, size, u"%ls%ls", gObjectUINameBuffer, mSignB);
    } else if (wcslen(mSignB) == 0) {
        wcsncpy(out, mSignA, size);
    } else {
        swprintf(out, size, u"%ls %ls", mSignA, mSignB);
    }
}

// The first sign line; a line of two or more characters ending in two hyphens loses the last one.
void CObject::GetSignNameA(char16_t* out, uint32_t size) {
    const uint32_t length = wcslen(mSignA);
    if (length == 0) {
        swprintf(out, size, u"");
        return;
    }
    if (static_cast<int32_t>(length) > 1 && mSignA[length - 1] == u'-' && mSignA[length - 2] == u'-') {
        gObjectSignNameBuffer[0x20] = 0;
        swprintf(gObjectSignNameBuffer, 0x21, u"%ls", mSignA);
        gObjectSignNameBuffer[length - 1] = 0;
        swprintf(out, size, u"%ls", gObjectSignNameBuffer);
        return;
    }
    swprintf(out, size, u"%ls", mSignA);
}

void CObject::GetSignNameB(char16_t* out, uint32_t size) {
    swprintf(out, size, u"%ls", mSignB);
}

// A new first sign line. Outside the online game the gates' line is the park's too (the master save is created on first
// use). The action recorder gets the gates as 60000 and any other ride as its cell.
void CObject::SetSignNameA(const char16_t* text) {
    wcsncpy(mSignA, text, 0x21);
    if (TheGameType().mType != kGameTypeOnline && mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        if (gMasterSave == nullptr) gMasterSave = new (std::nothrow) CFSGlobalMasterSave;
        gMasterSave->SetSignNameA(text);
    }
    if (mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x50), 60000, static_cast<uint8_t>(wcslen(mSignA)), text);
    } else {
        const uint16_t cell = InfoCell(this);
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x50), cell, static_cast<uint8_t>(wcslen(mSignA)), text);
    }
}

// The second line, as SetSignNameA (action 0x55).
void CObject::SetSignNameB(const char16_t* text) {
    wcsncpy(mSignB, text, 0x21);
    if (TheGameType().mType != kGameTypeOnline && mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        if (gMasterSave == nullptr) gMasterSave = new (std::nothrow) CFSGlobalMasterSave;
        gMasterSave->SetSignNameB(text);
    }
    if (mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x55), 60000, static_cast<uint8_t>(wcslen(mSignB)), text);
    } else {
        const uint16_t cell = InfoCell(this);
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x55), cell, static_cast<uint8_t>(wcslen(mSignB)), text);
    }
}

// gameplay_leaves.cpp (main, unit v5L07): small leaves a park run reaches, from different Mac files: CellIsRide 0x1006d718
// (the inventory's Debug.cpp range), LocatorByeByeKid 0x101bf98c (ui_staffPanel.cpp), CObject::IsCoaster 0x10110cd8 (Object.cpp) and CAdvisor::FinalDestroy 0x1000a0ec (Advisor.cpp).

// 0x10110cd8 (76 bytes): the object's ride record has track type 3. The record is not tested for NULL (the Mac reads through it).
int32_t CObject::IsCoaster() const {
    const RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
    return info->Value(kRideSlot_WhichTrackType).mInteger == 3 ? 1 : 0;
}

// A ride on a track: the record's WhichTrackType is 1, 2 or 3 (each test fetches the record again).
int32_t CObject::IsTrackRide() const {
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 1) return 1;
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 2) return 1;
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 3) return 1;
    return 0;
}

int32_t CMsgRideWantsUpgrade::GetType() const { return 9; }
int32_t CMsgRideBrokenDown::GetType() const { return 7; }
int32_t CMsgRideCondemned::GetType() const { return 8; }
int32_t CMsgBlockedExit::GetType() const { return 0x14; }

int32_t CMsgRideDeleted::GetType() const { return 10; }
// CMsgRideDeleted's constructor (unit w210) is at the end of this file.
int32_t CMsgNewRide::GetType() const { return 6; }

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

template<> int32_t LOADVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10111844, 236 bytes
}

template<> int32_t SAVEVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10111a98, 252 bytes
}
