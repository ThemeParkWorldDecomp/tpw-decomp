// guard.cpp: name proven by __sinit_guard_cpp in the main application.

#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "VehicleSteer.h"
#include "World.h"
#include <cstdint>
#include <new>

// CGuard (main app, SimTheme Park 2.04b3, Guard.cpp; unit w182): both constructors 0x100fda68 and 0x100fdb08, Init 0x100fdc10, FinalDestroy
// 0x100fdd60, ReceiveMessage 0x100fdec0, Serialise 0x100fdf94, ProsecutePerp 0x100fe134, IsAvailable 0x100fe2b0, IsCarrying 0x100fe300,
// UpdateTurn 0x100fe344, ChooseNextAction 0x100fe430, ModelState 0x100fe4e8, SetState 0x100fe5a8 and the five Process states 0x100fe774 to
// 0x100feebc.
//
// A guard is a staff member (the Mac builds the CStaff part with the CStaff constructor) with a state word at +0x19c, the name at +0x1a0, the
// list link at +0x20c, the kid being chased at +0x20e and a countdown at +0x210. The host's CGuard is plain data at those offsets; the
// untranslated CStaff, CPeep and CPerson members are called on it (and on the kid's row) through casts of the address, as the other staff
// classes do. The "idle" and "patrolling" tails share the ChooseNextAction decision; it is written once as ChooseWalkOrWait (not a Mac function).

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kStaffModel = 7;            // the model the CStaff constructor is given
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStateReleasing = 7;        // SetState only: lets the carried kid go
constexpr int32_t kStateProsecuting = 0x10;
constexpr int32_t kStateWalkingToEntrance = 0x12;
constexpr int32_t kStateReturningToPatrol = 0x13;
constexpr int32_t kNameCount = 0x23;
constexpr int32_t kNameCapacity = 0x20;
constexpr int32_t kTextResourceIndex = 14;    // gText + 0x584
constexpr int32_t kSoundBankIndex = 4;        // gSoundBanks + 0x10
constexpr int32_t kSoundIdle = 0xa7;
constexpr int32_t kSoundPatrolling = 0xa6;
constexpr int32_t kSoundChase = 0x88;
constexpr int32_t kSoundCaught = 0x89;
constexpr int32_t kMsgDeath = 0x1b;
constexpr int32_t kRegionEffector = 3;
constexpr int32_t kSpriteScriptSpawn = 3;
constexpr int32_t kScriptCaught = 0x16;
constexpr int32_t kScriptWalkingKid = 0x18;
constexpr int32_t kGuardSpriteSet = 7;
constexpr uint16_t kChaseSpeedBonus = 25;      // halfword 1 of the table at 0x102a0d54
constexpr uint16_t kNoSpeedBonus = 0;          // halfword 0
constexpr uint32_t kNullThing = 0;             // the words at 0x102a09b4, 0x102a0a08 and 0x102a0a5c

const char kRowAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

CPeep* AsPeep(CGuard* g) { return reinterpret_cast<CPeep*>(g); }
CStaff* AsStaff(CGuard* g) { return reinterpret_cast<CStaff*>(g); }

MapId CellOfThing(const CThing& t) {
    return MapId{static_cast<uint16_t>(static_cast<uint16_t>(t.mCellX) + static_cast<uint16_t>(t.mCellY) * 0x80 + 1)};
}
MapId CellOf(const CGuard* g) { return CellOfThing(g->mThing); }

// The row of a thing id, after the check that row 0 is free.
void* RowOf(ThingId id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowAssert);
    return gThingArray[id.mIndex].mThing;
}
CPerson* PersonOf(void* row) { return static_cast<CPerson*>(row); }

// The decision ChooseNextAction ends with, after DoGeneralStaffThing said no.
void ChooseWalkOrWait(CGuard* g) {
    if ((gWorld->mTurn & 3) != 0 && AsPeep(g)->SetRandomDestination() != 0) {
        g->SetState(static_cast<CStaff::StaffState>(kStatePatrolling));
    } else {
        g->SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
}

// The guard lets the kid he carries go (FinalDestroy and SetState 7): the id is read, checked and the row looked up twice.
void ReleaseCarriedKid(CGuard* g) {
    if (g->mState == kStateWalkingToEntrance && g->mPerp.mIndex != kNullThing) {
        ThingId id = g->mPerp;
        void* row = RowOf(id);
        if (row != nullptr) {
            id = g->mPerp;
            row = RowOf(id);
            PersonOf(row)->GetDebagged(CellOf(g));
            g->mPerp.mIndex = 0;
        }
    }
}

}  // namespace

// 0x100fda68
CGuard::CGuard(MapId cell) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel));
    Init();
}

// 0x100fdb08
CGuard::CGuard(MapId cell, int32_t payGrade, uint8_t flag, int32_t value) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel), static_cast<CStaff::PayGrade>(payGrade), flag);
    GAME_ASSERTMSG(value >= 0 && value < kNameCount, "Name of guard out of bounds");
    static_cast<TbITable*>(gText.mResourcePointers[kTextResourceIndex])->FormatString(mName, kNameCapacity, static_cast<uint32_t>(value));
    Init();
}

// 0x100fdc10
void CGuard::Init() {
    mNext.mIndex = 0;
    gWorld->Register(this);
    mSpriteSet = kGuardSpriteSet;
    mSpriteIndex = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(mSpriteSet));
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kSpriteScriptSpawn), mSpriteSet, mSpriteIndex);
    gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(kRegionEffector), CellOf(this));
    mPerp.mIndex = 0;
    mProsecutionTimestamp = 0;
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->Subscribe(mThing, static_cast<MsgType>(kMsgDeath));
    ChooseNextAction();
}

// 0x100fdd60
void CGuard::FinalDestroy() {
    gWorld->Unregister(this);
    const MapId last = AsPeep(this)->LastCellVisited();
    gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(kRegionEffector), last);
    ReleaseCarriedKid(this);
    AsStaff(this)->FinalDestroy();
}

// 0x100fdec0: a death message for the kid being chased drops the chase; the staff's own handling follows either way.
void CGuard::ReceiveMessage(const CMsg& message) {
    if (message.GetType() == kMsgDeath && static_cast<const CMsgDeath&>(message).mThingId == mPerp.mIndex) {
        CTRACE(2, "Guard: kid I was chasing died\n");
        mPerp.mIndex = 0;
    }
    AsStaff(this)->ReceiveMessage(message);
}

// 0x100fdf94: the staff block, then (for saves of version 0 and 1) the kid, the countdown and the list link. Nonzero is a failure.
int32_t CGuard::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsStaff(this)->Serialise(file, save, version, unused) != 0) {
        return 1;
    }
    if (save == 0) {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT(file, &mPerp, "mPerp", 0) != 0) return 1;
            if (LOADVAR(file, &mProsecutionTimestamp, "mProsecutionTimestamp", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (SAVEVARSTRUCT(file, &mPerp, "mPerp", 0) != 0) return 1;
        if (SAVEVAR(file, &mProsecutionTimestamp, "mProsecutionTimestamp", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
    }
    return 0;
}

// 0x100fe134
void CGuard::ProsecutePerp(ThingId perp) {
    if (AsStaff(this)->TooTiredToWork() != 0) {
        CTRACE(2, "Guard %d: Too tired to chase kids\n", static_cast<uint32_t>(mThing.GetThingId().mIndex));
        return;
    }
    AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundChase);
    mPerp = perp;
    void* row = RowOf(perp);
    if (AsPeep(this)->SetDest(CellOfThing(*static_cast<CThing*>(row))) == 0) {
        CTRACE(2, "Could not get to kid to prosecute him\n");
    } else {
        SetState(static_cast<CStaff::StaffState>(kStateProsecuting));
    }
}

// 0x100fe2b0
bool CGuard::IsAvailable() const { return mState == kStatePatrolling || mState == kStateIdle; }

// 0x100fe300
bool CGuard::IsCarrying() const { return mState == kStateWalkingToEntrance; }

// 0x100fe344
void CGuard::UpdateTurn() {
    const MapId last = AsPeep(this)->LastCellVisited();
    if (last.mIndex != CellOf(this).mIndex) {
        gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(kRegionEffector), last);
        gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(kRegionEffector), CellOf(this));
    }
    AsStaff(this)->UpdateTurn();
}

// 0x100fe430
void CGuard::ChooseNextAction() {
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    ChooseWalkOrWait(this);
}

// 0x100fe4e8
void CGuard::ModelState() {
    switch (static_cast<uint32_t>(mState)) {
    case kStateIdle: ProcessIdle(); return;
    case kStatePatrolling: ProcessPatrolling(); return;
    case kStateWalkingToRest: AsStaff(this)->ProcessWalkingToRestArea(); return;
    case kStateResting: AsStaff(this)->ProcessResting(); return;
    case kStateProsecuting: ProcessProsecuting(); return;
    case kStateWalkingToEntrance: ProcessWalkingToEntrance(); return;
    case kStateReturningToPatrol: ProcessReturningToPatrol(); return;
    default: AsStaff(this)->ModelState(); return;
    }
}

// 0x100fe5a8: states 7, 0x10, 0x12 and 0x13 are the guard's own and skip the staff's SetState; 0 also forgets the kid.
void CGuard::SetState(CStaff::StaffState newState) {
    const uint32_t state = static_cast<uint32_t>(newState);
    switch (state) {
    case kStateIdle:
        mPerp.mIndex = 0;
        AsStaff(this)->SetState(newState);
        break;
    case kStateReleasing:
        ReleaseCarriedKid(this);
        break;
    case kStateProsecuting:
        mSpeedBonus = kChaseSpeedBonus;
        mProsecutionTimestamp = static_cast<uint32_t>(
            gMainBalance.mValues[kMainBalanceGuardWorkDuration + kMainBalanceGuardGradeStride * mGrade].mInteger);
        break;
    case kStateWalkingToEntrance:
    case kStateReturningToPatrol:
        mSpeedBonus = kNoSpeedBonus;
        break;
    default:
        mSpeedBonus = kNoSpeedBonus;
        AsStaff(this)->SetState(newState);
        break;
    }
    mState = static_cast<int32_t>(newState);
}

// 0x100fe774
void CGuard::ProcessIdle() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundIdle);
    }
    if (gWorld->mTurn <= static_cast<uint32_t>(mStateTurn + gMainBalance.mValues[kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade].mInteger)) {
        return;
    }
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    ChooseWalkOrWait(this);
}

// 0x100fe888
void CGuard::ProcessPatrolling() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundPatrolling);
    }
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks == 0 || walks == 2) {
        if (AsStaff(this)->DoGeneralStaffThing() == 0) {
            ChooseWalkOrWait(this);
        }
    } else {
        AsStaff(this)->GetTiredPatrolling();
    }
}

// 0x100fe99c
void CGuard::ProcessProsecuting() {
    AsStaff(this)->GetTiredWorking();
    bool abandon = false;
    if (mPerp.mIndex == kNullThing || mProsecutionTimestamp == 0) {
        CTRACE(2, "Guard: kid was NULL or timestamp expired\n");
        abandon = true;
    } else {
        mProsecutionTimestamp = mProsecutionTimestamp - 1;
        ThingId id = mPerp;
        CPerson* kid = PersonOf(RowOf(id));
        const int32_t walks = AsPeep(this)->PeepWalks();
        const MapId mine = CellOf(this);
        const MapId theirs = CellOfThing(*reinterpret_cast<CThing*>(kid));
        bool catchIt = false;
        if (walks == 0) {
            if (mine.mIndex != theirs.mIndex) {
                if (AsPeep(this)->SetDest(theirs) != 0) {
                    CTRACE(2, "Guard: Moving to kid's new position\n");
                    return;
                }
                CTRACE(2, "Guard: Cannot get to perp's position\n");
                abandon = true;
            } else {
                catchIt = true;
            }
        } else if (walks == 2) {
            abandon = true;
        } else if (mine.mIndex != theirs.mIndex) {
            return;
        } else {
            catchIt = true;
        }
        if (catchIt) {
            id = mPerp;
            if (PersonOf(RowOf(id))->IsCatchable() == 0) {
                CTRACE(2, "Guard: kid not catchable\n");
                abandon = true;
            } else {
                CTRACE(2, "Guard: Caught a kid\n");
                mSpeedBonus = kNoSpeedBonus;
                Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptCaught));
                gWorld->GetParkAnalyser()->IveMisbehaved(2);
                AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundCaught);
                kid->GetBagged();
                MapId booth = gWorldMap->GetTicketBoothPos(static_cast<int32_t>(gWorld->mTurn & 1));
                if (AsPeep(this)->SetDest(booth) != 0) {
                    SetState(static_cast<CStaff::StaffState>(kStateWalkingToEntrance));
                    return;
                }
                mF188 = 1;
                booth = gWorldMap->GetTicketBoothPos(static_cast<int32_t>(gWorld->mTurn & 1));
                if (AsPeep(this)->SetDest(booth) != 0) {
                    SetState(static_cast<CStaff::StaffState>(kStateWalkingToEntrance));
                    return;
                }
                CTRACE(2, "!!! Guard can't get to the entrance, releasing kid !!!\n");
                kid->GetDebagged(CellOf(this));
                abandon = true;
            }
        }
    }
    if (abandon) {
        CTRACE(2, "Guard: abandoned chase\n");
        gWorld->GetParkAnalyser()->IveMisbehaved(0);
        mSpeedBonus = kNoSpeedBonus;
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
        mPerp.mIndex = 0;
    }
}

// 0x100fed5c
void CGuard::ProcessWalkingToEntrance() {
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks != 0 && walks != 2) {
        return;
    }
    Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptWalkingKid));
    ThingId id = mPerp;
    PersonOf(RowOf(id))->GetDebagged(CellOf(this));
    mF188 = 0;
    const MapId entrance = gWorldMap->GetParkEntrancePos(static_cast<int32_t>(gWorld->mTurn & 1));
    if (AsPeep(this)->SetDest(entrance) == 0) {
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    } else {
        SetState(static_cast<CStaff::StaffState>(kStateReturningToPatrol));
    }
}

// 0x100feebc
void CGuard::ProcessReturningToPatrol() {
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks != 0 && walks != 2) {
        return;
    }
    const MapId entrance = gWorldMap->GetParkEntrancePos(static_cast<int32_t>(gWorld->mTurn & 1));
    if (AsPeep(this)->SetDest(entrance) == 0) {
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    } else {
        SetState(static_cast<CStaff::StaffState>(kStatePatrolling));
    }
}

// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).

int32_t gGuardSinitA;        // 0x1034c204
int32_t gGuardSinitB;        // 0x1034c208

void __sinit_guard_cpp() {
    gGuardSinitA = 0x10000;
    gGuardSinitB = 0;
}
