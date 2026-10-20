// CHandyman (main app, SimTheme Park 2.04b3, Handyman.cpp; unit w184): both constructors 0x100fefd0 and 0x100ff0a4, Init 0x100ff1b8,
// FinalDestroy 0x100ff2e0, ReceiveMessage 0x100ff3c4, Serialise 0x100ff4c0, UpdateTurn 0x100ff6b4, ChooseNextAction 0x100ff7e4, SetState
// 0x100ffb70, ModelState 0x100ffca8, the six Process states 0x100ffd78 to 0x101004bc, FindNearestDirtyToilet 0x10100580 and
// __sinit_Handyman_cpp 0x1010084c.
//
// A handyman is a staff member (the Mac builds the CStaff part with the CStaff constructor) that tidies litter and cleans toilets. It keeps the
// map cell with litter it is walking to (+0x214), the toilet it is walking to (+0x216), the turn it started cleaning (+0x210) and the list link
// (+0x20c). The host's CHandyman is plain data at those offsets; the untranslated CStaff, CPeep and CObject members are called on it (and on the
// toilet's row) through casts of the address, as the other staff classes do.
#include <cstdint>
#include <cstdlib>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Serialise.h"
#include "Text.h"
#include "TbITable.h"
#include "TPSound.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

// The constructor's function static and the two words the static initialiser stores (nothing reads the latter two).
uint8_t gHandymanSpriteGuard = 0;
int32_t gHandymanSprite = 0;
uint32_t gHandymanSinitLow = 0;
uint32_t gHandymanSinitHigh = 0x10000;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kStaffModel = 5;            // the model the CStaff constructor is given
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStateWalkingToLitter = 8;
constexpr int32_t kStateCleaningLitter = 9;
constexpr int32_t kStateWalkingToToilet = 10;
constexpr int32_t kStateCleaningToilet = 11;
constexpr int32_t kNameCount = 0x23;
constexpr int32_t kNameCapacity = 0x20;
constexpr int32_t kTextResourceIndex = 11;    // gText + 0x578
constexpr int32_t kSoundBankIndex = 4;        // gSoundBanks + 0x10
constexpr int32_t kSoundPatrolling = 0xa0;
constexpr int32_t kSoundIdle = 0xa1;
constexpr int32_t kMsgRideDeleted = 10;
constexpr int32_t kSpriteScriptSpawn = 3;
constexpr int32_t kScriptWalking = 9;
constexpr int32_t kScriptCleaning = 0x11;
constexpr int32_t kScriptStanding = 3;
constexpr int32_t kHandymanSpriteSet = 5;
constexpr uint16_t kChaseSpeedBonus = 25;     // halfword 1 of the table at 0x102a0d54
constexpr uint32_t kNullThing = 0;            // the words at 0x102a0a5e, 0x102a0a60, 0x102a0a62, 0x102a0a64, 0x102a0a98, 0x102a0a9a and 0x102a0a9c
constexpr uint32_t kMapCellSize = 0x44;

const char kRowAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

CPeep* AsPeep(CHandyman* h) { return reinterpret_cast<CPeep*>(h); }
CStaff* AsStaff(CHandyman* h) { return reinterpret_cast<CStaff*>(h); }

MapId CellOfThing(const CThing& t) {
    return MapId{static_cast<uint16_t>(static_cast<uint16_t>(t.mCellX) + static_cast<uint16_t>(t.mCellY) * 0x80 + 1)};
}
MapId CellOf(const CHandyman* h) { return CellOfThing(h->mThing); }

// The row of a thing id, after the check that row 0 is free.
void* RowOf(ThingId id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowAssert);
    return gThingArray[id.mIndex].mThing;
}
CObject* ObjectOf(void* row) { return static_cast<CObject*>(row); }

// The map cell of a cell id: entry (id - 1), 0x44 bytes each.
CMapCell* MapCellOf(MapId id) {
    const int32_t offset = (static_cast<int32_t>(id.mIndex) - 1) * static_cast<int32_t>(kMapCellSize);
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gWorldMap) + static_cast<intptr_t>(offset));
}

int32_t BalanceAt(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

// The word "x" and "y" a cell id has on the map (cell ids start at 1).
int32_t CellX(MapId id) { return static_cast<int32_t>((static_cast<uint32_t>(id.mIndex) - 1u) & 0x7fu); }
int32_t CellY(MapId id) { return static_cast<int32_t>((static_cast<uint32_t>(id.mIndex) - 1u) >> 7); }

}  // namespace

// 0x100fefd0: the sprite index is a function static, set to 0 on the first call.
CHandyman::CHandyman(MapId cell) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel));
    if (static_cast<int8_t>(gHandymanSpriteGuard) == 0) {
        gHandymanSprite = 0;
        gHandymanSpriteGuard = 1;
    }
    Init(static_cast<uint8_t>(gHandymanSprite));
}

// 0x100ff0a4
CHandyman::CHandyman(MapId cell, int32_t payGrade, uint8_t flag, int32_t value) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel), static_cast<CStaff::PayGrade>(payGrade), flag);
    GAME_ASSERTMSG(value >= 0 && value < kNameCount, "Name of handyman out of bounds");
    static_cast<TbITable*>(gText.mResourcePointers[kTextResourceIndex])->FormatString(mName, kNameCapacity, static_cast<uint32_t>(value));
    Init(flag);
}

// 0x100ff1b8
void CHandyman::Init(uint8_t sprite) {
    mNext.mIndex = 0;
    gWorld->Register(this);
    mTimeStartedCleaning = 0;
    mTargetLitterCell.mIndex = 0;
    mToiletToClean.mIndex = 0;
    mSpriteSet = kHandymanSpriteSet;
    mSpriteIndex = sprite;
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kSpriteScriptSpawn), mSpriteSet, mSpriteIndex);
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->Subscribe(mThing, static_cast<MsgType>(kMsgRideDeleted));
    ChooseNextAction();
}

// 0x100ff2e0: the toilet being walked to is told it is no longer expected.
void CHandyman::FinalDestroy() {
    gWorld->Unregister(this);
    if (mToiletToClean.mIndex != kNullThing) {
        ThingId id = mToiletToClean;
        void* row = RowOf(id);
        ObjectOf(row)->UnassignStaffMember();
    }
    AsStaff(this)->FinalDestroy();
}

// 0x100ff3c4: a deleted ride that is the toilet being walked to drops the plan; the staff's own handling follows either way.
void CHandyman::ReceiveMessage(const CMsg& message) {
    if (message.GetType() == kMsgRideDeleted && static_cast<const CMsgRideDeleted&>(message).mThingId == mToiletToClean.mIndex) {
        CTRACE(2, "Handyman: toilet I was cleaning was deleted\n");
        mToiletToClean.mIndex = 0;
        if (mState == kStateCleaningToilet) {
            Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptStanding));
        }
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
    AsStaff(this)->ReceiveMessage(message);
}

// 0x100ff4c0: the staff block, then (for saves of version 0 and 1) the litter cell, the cleaning turn, the toilet and the list link. Nonzero is a failure.
int32_t CHandyman::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsStaff(this)->Serialise(file, save, version, unused) != 0) {
        return 1;
    }
    MacULong* started = reinterpret_cast<MacULong*>(&mTimeStartedCleaning);
    if (save == 0) {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT(file, &mTargetLitterCell, "mTargetLitterCell", 0) != 0) return 1;
            if (LOADVAR(file, started, "mTimeStartedCleaning", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mToiletToClean, "mToiletToClean", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (SAVEVARSTRUCT(file, &mTargetLitterCell, "mTargetLitterCell", 0) != 0) return 1;
        if (SAVEVAR(file, started, "mTimeStartedCleaning", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mToiletToClean, "mToiletToClean", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
    }
    return 0;
}

// 0x100ff6b4: an idle handyman looks for litter every (idle interval) turns; a turn count that is a multiple of a zero interval is the
// turn count itself (the Mac's divwu by zero leaves the quotient 0, so the remainder is the whole number).
void CHandyman::UpdateTurn() {
    AsStaff(this)->UpdateTurn();
    if (mState == kStateIdle) {
        CWorld* world = gWorld;
        const uint32_t interval = static_cast<uint32_t>(BalanceAt(kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade));
        const uint32_t turn = world->mTurn;
        const uint32_t remainder = interval != 0 ? turn % interval : turn;
        if (remainder == 0) {
            const MapId cell = CellOf(this);
            const ThingId id = mThing.GetThingId();
            CParkAnalyser* analyser = world->GetParkAnalyser();
            const MapId found = analyser->GetNearbyLitteredCell(id, cell, BalanceAt(kMainBalanceHandymanDetectionRange + kMainBalanceHandymanGradeStride * mGrade));
            if (found.mIndex != kNullThing) {
                ChooseNextAction();
            }
        }
    }
    Thoughts()->ProcessThoughtBubbles();
}

// 0x100ff7e4
void CHandyman::ChooseNextAction() {
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    if (AsStaff(this)->TooTiredToWork() != 0) {
        CTRACE(2, "Too tired to tidy stuff up\n");
    } else {
        const MapId cell = CellOf(this);
        CWorld* world = gWorld;
        const ThingId id = mThing.GetThingId();
        CParkAnalyser* analyser = world->GetParkAnalyser();
        mTargetLitterCell = analyser->GetNearbyLitteredCell(id, cell, BalanceAt(kMainBalanceHandymanDetectionRange + kMainBalanceHandymanGradeStride * mGrade));
        if (mTargetLitterCell.mIndex == kNullThing) {
            mToiletToClean = FindNearestDirtyToilet();
            if (mToiletToClean.mIndex != kNullThing) {
                CTRACE(2, "Handyman found dirty loo, walking there now\n");
                SetState(static_cast<CStaff::StaffState>(kStateWalkingToToilet));
                return;
            }
        } else {
            CWorld* seeded = gWorld;
            seeded->SeedRand(mTargetLitterCell.mIndex);
            int32_t first = static_cast<int32_t>(seeded->GetRand() & 0x7f);
            int32_t second = static_cast<int32_t>(gWorld->GetRand() & 0x7f);
            if (static_cast<uint8_t>(first) < 1) first = 1;
            if (static_cast<uint8_t>(first) > 9) first = 9;
            if (static_cast<uint8_t>(second) < 1) second = 1;
            if (static_cast<uint8_t>(second) > 9) second = 9;
            const uint32_t t = mTargetLitterCell.mIndex;
            const MapPos x{static_cast<uint8_t>((t - 1u) & 0x7fu), static_cast<uint8_t>(first * 13)};
            const MapPos y{static_cast<uint8_t>((t - 1u) >> 7), static_cast<uint8_t>(second * 13)};
            const int32_t result = AsPeep(this)->SetDest(x, y);
            if (result == 1) {
                CMapCell* target = MapCellOf(mTargetLitterCell);
                target->MarkForLitterCollection(mThing.GetThingId());
                SetState(static_cast<CStaff::StaffState>(kStateWalkingToLitter));
                return;
            }
            const MapId here = CellOf(this);
            const int32_t hx = CellX(here);
            const int32_t hy = CellY(here);
            const ThingId me = mThing.GetThingId();
            CTRACE(2, "Handyman %d at (%d, %d): ", static_cast<uint32_t>(me.mIndex), hx, hy);
            TRACE("**** Can't set destination to that litter cell! ****\n");
        }
    }
    if (AsPeep(this)->SetRandomDestination() != 0) {
        SetState(static_cast<CStaff::StaffState>(kStatePatrolling));
    } else {
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
}

// 0x100ffb70: states 8 and 10 head for litter or a toilet at the chase speed, 9 and 11 start the clock; the rest are the staff's.
void CHandyman::SetState(CStaff::StaffState newState) {
    const int32_t state = static_cast<int32_t>(newState);
    switch (state) {
    case kStateWalkingToLitter:
    case kStateWalkingToToilet:
        mSpeedBonus = kChaseSpeedBonus;
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptWalking));
        break;
    case kStateCleaningLitter:
    case kStateCleaningToilet:
        mTimeStartedCleaning = gWorld->mTurn;
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptCleaning));
        break;
    default:
        AsStaff(this)->SetState(newState);
        break;
    }
    mState = state;
}

// 0x100ffca8
void CHandyman::ModelState() {
    switch (static_cast<uint32_t>(mState)) {
    case kStateIdle: ProcessIdle(); return;
    case kStatePatrolling: ProcessPatrolling(); return;
    case kStateWalkingToRest: AsStaff(this)->ProcessWalkingToRestArea(); return;
    case kStateResting: AsStaff(this)->ProcessResting(); return;
    case kStateWalkingToLitter: ProcessWalkingToLitter(); return;
    case kStateCleaningLitter: ProcessCleaningLitter(); return;
    case kStateWalkingToToilet: ProcessWalkingToCleanToilet(); return;
    case kStateCleaningToilet: ProcessCleaningToilet(); return;
    default: AsStaff(this)->ModelState(); return;
    }
}

// 0x100ffd78
void CHandyman::ProcessPatrolling() {
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

// 0x100ffe38
void CHandyman::ProcessWalkingToLitter() {
    ThingId me = mThing.GetThingId();
    if (MapCellOf(mTargetLitterCell)->mLitterCollector != me.mIndex) {
        CTRACE(2, "UNUSUAL INCIDENT!! Handyman: didn't get to litter in time, someone else cleaning it, so resume patrolling!!\n");
        mTargetLitterCell.mIndex = 0;
        ChooseNextAction();
        return;
    }
    const int32_t x = CellX(mTargetLitterCell);
    const int32_t y = CellY(mTargetLitterCell);
    const ThingId id = mThing.GetThingId();
    GAME_ASSERTMSG(MapCellOf(mTargetLitterCell)->mLitter != 0, "Handyman %d: the litter in cell (%d, %d) has gone en-route",
                   static_cast<uint32_t>(id.mIndex), x, y);
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks == 0) {
        if (MapCellOf(mTargetLitterCell)->mLitter == 0) {
            GAME_ASSERTMSG(0, "*** Arrived at littered cell but no litter! ****");
            mTargetLitterCell.mIndex = 0;
            ChooseNextAction();
        } else {
            AsPeep(this)->Teleport(mPosX, mPosY);
            SetState(static_cast<CStaff::StaffState>(kStateCleaningLitter));
        }
    } else if (walks == 2) {
        CTRACE(2, "Handyman - my route to the litter was blocked\n");
        mTargetLitterCell.mIndex = 0;
        ChooseNextAction();
    }
}

// 0x10100050
void CHandyman::ProcessCleaningLitter() {
    AsStaff(this)->GetTiredWorking();
    if (static_cast<uint32_t>(mTimeStartedCleaning + static_cast<uint32_t>(BalanceAt(kMainBalanceHandymanWorkDuration + kMainBalanceHandymanGradeStride * mGrade))) < gWorld->mTurn) {
        const MapId own = CellOf(this);
        if (MapCellOf(own)->mLitter == 0) {
            GAME_ASSERTMSG(0, "Cell cleaned by someone else while I was cleaning it!\n");
            mTargetLitterCell.mIndex = 0;
            ChooseNextAction();
        } else {
            const int32_t x = CellX(own);
            const int32_t y = CellY(own);
            const ThingId id = mThing.GetThingId();
            CTRACE(2, "Handyman %d: cleaning litter in cell (%d, %d)", static_cast<uint32_t>(id.mIndex), x, y);
            MapCellOf(mTargetLitterCell)->RemoveLitter();
            mTargetLitterCell.mIndex = 0;
            ChooseNextAction();
        }
    }
}

// 0x10100230
void CHandyman::ProcessWalkingToCleanToilet() {
    const int32_t walks = AsPeep(this)->PeepWalks();
    if (walks == 0) {
        ThingId id = mToiletToClean;
        CObject* toilet = ObjectOf(RowOf(id));
        if (toilet->mF32 == CellOf(this).mIndex) {
            const ThingId me = mThing.GetThingId();
            const ThingId assigned = toilet->GetAssignedStaffMember();
            if (assigned.mIndex == me.mIndex) {
                SetState(static_cast<CStaff::StaffState>(kStateCleaningToilet));
            } else {
                GAME_ASSERTMSG(0, "Handyman: got to the loo I was going to clean, but it wasn't expecting me\n");
                SetState(static_cast<CStaff::StaffState>(kStateIdle));
            }
        } else {
            CTRACE(2, "Handyman: got to the loo I was going to clean, but it isn't here any more\n");
            SetState(static_cast<CStaff::StaffState>(kStateIdle));
        }
    } else if (walks == 2) {
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
}

// 0x101003ac: mToiletToClean is not cleared when the toilet is done.
void CHandyman::ProcessCleaningToilet() {
    AsStaff(this)->GetTiredWorking();
    if (static_cast<uint32_t>(mTimeStartedCleaning + static_cast<uint32_t>(BalanceAt(kMainBalanceHandymanWorkDuration + kMainBalanceHandymanGradeStride * mGrade))) < gWorld->mTurn) {
        ThingId id = mToiletToClean;
        CObject* toilet = ObjectOf(RowOf(id));
        toilet->CleanFully();
        toilet->Reopen();
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptStanding));
        SetState(static_cast<CStaff::StaffState>(kStateIdle));
    }
}

// 0x101004bc
void CHandyman::ProcessIdle() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundIdle);
    }
    if (static_cast<uint32_t>(mStateTurn + static_cast<uint32_t>(BalanceAt(kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade))) < gWorld->mTurn) {
        ChooseNextAction();
    }
}

// 0x10100580: walks the world's object list for the nearest toilet (provides relief) that is waiting for this handyman or nobody, is flagged for
// maintenance (any distance) or is dirty and within the detection range, and that the handyman can walk to; the winner is assigned to it.
ThingId CHandyman::FindNearestDirtyToilet() {
    uint32_t best = 0xffffffffu;
    ThingId bestId{0};
    uint16_t current = gWorld->mF1DA746;
    while (current != kNullThing) {
        ThingId id{current};
        CObject* object = ObjectOf(RowOf(id));
        if ((object->mFlags & 1) != 0) {
            bool skip = false;
            if (object->GetAssignedStaffMember().mIndex != kNullThing) {
                const ThingId me = mThing.GetThingId();
                const ThingId assigned = object->GetAssignedStaffMember();
                if (assigned.mIndex != me.mIndex) skip = true;
            }
            if (!skip) {
                const MapId objectCell = CellOfThing(*reinterpret_cast<CThing*>(object));
                const MapId myCell = CellOf(this);
                const uint32_t dx = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(
                    static_cast<uint32_t>(CellX(objectCell)) - static_cast<uint32_t>(CellX(myCell))))));
                const uint32_t dy = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(
                    static_cast<uint32_t>(CellY(objectCell)) - static_cast<uint32_t>(CellY(myCell))))));
                const uint32_t distance = dx * dx + dy * dy;
                bool candidate = false;
                if (object->IsFlaggedForMaintenance() != 0 && distance < best) {
                    candidate = true;
                } else if (object->IsDirty() != 0) {
                    const uint32_t range = static_cast<uint32_t>(BalanceAt(kMainBalanceHandymanDetectionRange + kMainBalanceHandymanGradeStride * mGrade));
                    if (distance < range * range) candidate = true;
                }
                if (candidate && AsPeep(this)->SetDest(MapId{object->mF32}) != 0) {
                    best = distance;
                    bestId = id;
                }
            }
        }
        current = object->mF0C;
    }
    if (bestId.mIndex != kNullThing) {
        ThingId id = bestId;
        CObject* toilet = ObjectOf(RowOf(id));
        const ThingId me = mThing.GetThingId();
        toilet->AssignStaffMember(me);
    }
    return bestId;
}
