// CMechanic (main app, SimTheme Park 2.04b3, Mechanic.cpp; unit w187): both constructors 0x1010409c and 0x10104170, Init 0x10104284,
// FinalDestroy 0x101043b0, ReceiveMessage 0x10104508, Serialise 0x101045e8, SetState 0x1010478c, ModelState 0x10104928, UpdateTurn 0x101049e0,
// ChooseNextAction 0x10104a30, ProcessPatrolling 0x10104ca4, ProcessWalkingToBreakdown 0x10104d64, ProcessRepairing 0x10104fd4, ProcessIdle
// 0x10105114 and __sinit_mechanic_cpp 0x101051d8.
//
// A mechanic is a staff member (the Mac builds the CStaff part with the CStaff constructor) that repairs rides. It keeps the ride it is walking to or
// repairing (+0x214), the turns left to repair it (+0x210) and the list link (+0x20c). The host's CMechanic is plain data at those offsets; the
// untranslated CStaff, CPeep and CObject members are called on it (and on the ride's row) through casts of the address, as the other staff classes do.
#include <cstdint>
#include <cstdlib>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "Serialise.h"
#include "Text.h"
#include "TbITable.h"
#include "TPSound.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

// The constructor's function static and the two words the static initialiser stores (nothing reads the latter two). The names are invented.
uint8_t gMechanicSpriteGuard = 0;
int32_t gMechanicSprite = 0;
uint32_t gMechanicSinitLow = 0;
uint32_t gMechanicSinitHigh = 0x10000;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kStaffModel = 4;            // the model the CStaff constructor is given
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStateWalkingToBreakdown = 12;
constexpr int32_t kStateRepairing = 13;
constexpr int32_t kNameCount = 0x23;
constexpr int32_t kNameCapacity = 0x20;
constexpr int32_t kTextResourceIndex = 12;    // gText + 0x57c
constexpr int32_t kSoundBankIndex = 4;        // gSoundBanks + 0x10
constexpr int32_t kSoundPatrolling = 0xa2;
constexpr int32_t kSoundIdle = 0xa3;
constexpr int32_t kMsgRideDeleted = 10;
constexpr int32_t kSpriteScriptSpawn = 3;
constexpr int32_t kScriptWalking = 9;
constexpr int32_t kScriptRepairing = 0x12;
constexpr int32_t kMechanicSpriteSet = 6;
constexpr int32_t kAdvisorBreakdown = 0x46;
constexpr uint32_t kNullThing = 0;            // the words at 0x102a0b5c and 0x102a0b98

const char kRowAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

CPeep* AsPeep(CMechanic* m) { return reinterpret_cast<CPeep*>(m); }
CStaff* AsStaff(CMechanic* m) { return reinterpret_cast<CStaff*>(m); }

// The row of a thing id, after the check that row 0 is free.
void* RowOf(ThingId id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowAssert);
    return gThingArray[id.mIndex].mThing;
}
CObject* ObjectOf(void* row) { return static_cast<CObject*>(row); }

int32_t BalanceAt(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

}  // namespace

// 0x1010409c: the sprite index is a function static, set to 0 on the first call.
CMechanic::CMechanic(MapId cell) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel));
    if (static_cast<int8_t>(gMechanicSpriteGuard) == 0) {
        gMechanicSprite = 0;
        gMechanicSpriteGuard = 1;
    }
    Init(static_cast<uint8_t>(gMechanicSprite));
}

// 0x10104170
CMechanic::CMechanic(MapId cell, int32_t payGrade, uint8_t flag, int32_t value) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel), static_cast<CStaff::PayGrade>(payGrade), flag);
    GAME_ASSERTMSG(value >= 0 && value < kNameCount, "Name of mechanic out of bounds");
    static_cast<TbITable*>(gText.mResourcePointers[kTextResourceIndex])->FormatString(mName, kNameCapacity, static_cast<uint32_t>(value));
    Init(flag);
}

// 0x10104284
void CMechanic::Init(uint8_t sprite) {
    mNext.mIndex = 0;
    gWorld->Register(this);
    mSpriteSet = kMechanicSpriteSet;
    mSpriteIndex = sprite;
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kSpriteScriptSpawn), mSpriteSet, mSpriteIndex);
    mObjectToRepair.mIndex = 0;
    mDurationOfRepair = 0;
    SetState(static_cast<CStaff::StaffState>(kStateIdle));
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->Subscribe(mThing, static_cast<MsgType>(kMsgRideDeleted));
    ChooseNextAction();
}

// 0x101043b0: the ride being walked to or repaired is told it is no longer expected (after a check that it agrees).
void CMechanic::FinalDestroy() {
    gWorld->Unregister(this);
    if (mObjectToRepair.mIndex != kNullThing) {
        ThingId id = mObjectToRepair;
        void* row = RowOf(id);
        CObject* object = ObjectOf(row);
        const ThingId repairing = mObjectToRepair;
        const ThingId first = mThing.GetThingId();
        const ThingId second = mThing.GetThingId();
        const ThingId assigned = object->GetAssignedStaffMember();
        GAME_ASSERTMSG(second.mIndex == assigned.mIndex, "Mechanic %d: I thought I was repairing object %d but it thinks otherwise\n",
                       static_cast<uint32_t>(first.mIndex), static_cast<uint32_t>(repairing.mIndex));
        object->UnassignStaffMember();
    }
    AsStaff(this)->FinalDestroy();
}

// 0x10104508: a deleted ride that is the one being repaired drops the plan; the staff's own handling follows either way.
void CMechanic::ReceiveMessage(const CMsg& message) {
    if (message.GetType() == kMsgRideDeleted && static_cast<const CMsgRideDeleted&>(message).mThingId == mObjectToRepair.mIndex) {
        CTRACE(2, "Mechanic: ride I was fixing was deleted\n");
        mObjectToRepair.mIndex = 0;
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
    AsStaff(this)->ReceiveMessage(message);
}

// 0x101045e8: the staff block, then (for saves of version 0 and 1) the repair duration, the ride and the list link. Nonzero is a failure.
int32_t CMechanic::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsStaff(this)->Serialise(file, save, version, unused) != 0) {
        return 1;
    }
    MacULong* duration = reinterpret_cast<MacULong*>(&mDurationOfRepair);
    if (save == 0) {
        if (version < 2 && version >= 0) {
            if (LOADVAR(file, duration, "mDurationOfRepair", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mObjectToRepair, "mObjectToRepair", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (SAVEVAR(file, duration, "mDurationOfRepair", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mObjectToRepair, "mObjectToRepair", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
    }
    return 0;
}

// 0x101049e0
void CMechanic::UpdateTurn() {
    AsStaff(this)->UpdateTurn();
}

// 0x10104a30
void CMechanic::ChooseNextAction() {
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    if (AsStaff(this)->TooTiredToWork() != 0) {
        CTRACE(2, "Too tired to fix rides\n");
    } else {
        CWorld* world = gWorld;
        const ThingId id = mThing.GetThingId();
        CMechanicHQ* hq = world->GetMechanicHQ();
        mObjectToRepair = hq->GetNewJob(id);
        if (mObjectToRepair.mIndex != kNullThing) {
            CTRACE(2, "Mechanic: found a ride that needs repairing\n");
            CObject* object = ObjectOf(RowOf(mObjectToRepair));
            if (AsPeep(this)->SetDest(MapId{object->mF32}) != 0) {
                object->AssignStaffMember(mThing.GetThingId());
                if (object->IsFlaggedForMaintenance() == 0 && object->RequiresUpgrade() == 0) {
                    const uint32_t type = object->mObjectType;
                    CWorld* queued = gWorld;
                    CAdvisorRequest request(static_cast<AdvisorMessage>(kAdvisorBreakdown), mObjectToRepair, type, -1, false, -1);
                    queued->GetAdvisor()->QueueRequest(request);
                }
                SetState(static_cast<CStaff::StaffState>(kStateWalkingToBreakdown));
                return;
            }
            CTRACE(2, "Mechanic: couldn't reach broken ride\n");
        }
    }
    if (AsPeep(this)->SetRandomDestination() != 0) {
        SetState(static_cast<CStaff::StaffState>(kStatePatrolling));
    } else {
        CTRACE(2, "Mechanic: Couldn't even move to random dest\n");
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
}

// 0x1010478c: 13 works out how long the repair takes, 12 walks; the rest are the staff's.
void CMechanic::SetState(CStaff::StaffState newState) {
    const int32_t state = static_cast<int32_t>(newState);
    if (state == kStateRepairing) {
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptRepairing));
        CObject* object = ObjectOf(RowOf(mObjectToRepair));
        if (object->RequiresUpgrade() != 0) {
            const uint32_t level = object->mF4C;
            RideInfo* info = object->GetDBInfo();
            const int32_t upgrade = info->Value(static_cast<RideBalanceSlot>(kRideSlot_Upgrades_DurationOfUpgrade + 16 * (static_cast<int32_t>(level) + 1))).mInteger;
            const int32_t work = BalanceAt(kMainBalanceMechanicWorkDuration + kMainBalanceMechanicGradeStride * mGrade);
            mDurationOfRepair = static_cast<uint32_t>(static_cast<int32_t>(static_cast<uint32_t>(work) * static_cast<uint32_t>(upgrade)));
        } else {
            const int32_t damage = ppc_fctiwz(static_cast<double>(object->mF40)) & 0xff;
            const int32_t work = BalanceAt(kMainBalanceMechanicWorkDuration + kMainBalanceMechanicGradeStride * mGrade);
            const int32_t product = static_cast<int32_t>(static_cast<uint32_t>(work) * static_cast<uint32_t>(100 - damage));
            mDurationOfRepair = static_cast<uint32_t>(product / 100);
        }
    } else if (state == kStateWalkingToBreakdown) {
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptWalking));
    } else {
        AsStaff(this)->SetState(newState);
    }
    mState = state;
}

// 0x10104928
void CMechanic::ModelState() {
    switch (static_cast<uint32_t>(mState)) {
    case kStateIdle: ProcessIdle(); return;
    case kStatePatrolling: ProcessPatrolling(); return;
    case kStateWalkingToRest: AsStaff(this)->ProcessWalkingToRestArea(); return;
    case kStateResting: AsStaff(this)->ProcessResting(); return;
    case kStateWalkingToBreakdown: ProcessWalkingToBreakdown(); return;
    case kStateRepairing: ProcessRepairing(); return;
    default: AsStaff(this)->ModelState(); return;
    }
}

// 0x10104ca4
void CMechanic::ProcessPatrolling() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundPatrolling);
    }
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks == 0 || walks == 2) {
        ChooseNextAction();
    } else {
        AsStaff(this)->GetTiredPatrolling();
    }
}

// 0x10104d64
void CMechanic::ProcessWalkingToBreakdown() {
    const ThingId me = mThing.GetThingId();
    ThingId id = mObjectToRepair;
    CObject* object = ObjectOf(RowOf(id));
    const ThingId assigned = object->GetAssignedStaffMember();
    if (assigned.mIndex != me.mIndex) {
        CTRACE(2, "UNUSUAL INCIDENT!! Mechanic: didn't get to breakdown in time, someone else assigned to repair it, so resume patrolling!!\n");
        mObjectToRepair.mIndex = 0;
        ChooseNextAction();
        return;
    }
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks == 0) {
        if (object->IsBrokenDown() == 0 && object->RequiresUpgrade() == 0 && object->IsFlaggedForMaintenance() == 0) {
            CTRACE(2, "\n\n\nMechanic: Arrived at ride but it's not broken down\n\n\n");
            ChooseNextAction();
        } else {
            SetState(static_cast<CStaff::StaffState>(kStateRepairing));
        }
    } else if (walks == 2) {
        mObjectToRepair.mIndex = 0;
        ChooseNextAction();
    }
}

// 0x10104fd4
void CMechanic::ProcessRepairing() {
    AsStaff(this)->GetTiredWorking();
    CObject* object = ObjectOf(RowOf(mObjectToRepair));
    if (mDurationOfRepair != 0) {
        mDurationOfRepair -= 1;
        return;
    }
    if (object->IsBrokenDown() != 0) {
        object->StartFixing();
        if (object->IsPlayingFixAnim() != 0) {
            return;
        }
        CTRACE(2, "Ride has finished fix anim\n");
    }
    object->RepairFully(mThing.GetThingId());
    mObjectToRepair.mIndex = 0;
    ChooseNextAction();
}

// 0x10105114
void CMechanic::ProcessIdle() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundIdle);
    }
    const uint32_t interval = static_cast<uint32_t>(BalanceAt(kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade));
    if (mStateTurn + interval < gWorld->mTurn) {
        ChooseNextAction();
    }
}
