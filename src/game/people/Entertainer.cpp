// Entertainer.cpp: name proven by __sinit_Entertainer_cpp in the main application.

#include "analyser.h"
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
#include "World.h"
#include <cstdint>
#include <new>

// CEntertainer (main app, SimTheme Park 2.04b3, Entertainer.cpp; unit w178): both constructors 0x100fb094 and 0x100fb224, Init 0x100fb33c,
// ReceiveMessage 0x100fb448, Serialise 0x100fb4a8, FinalDestroy 0x100fb600, UpdateTurn 0x100fb6b0, ChooseNextAction 0x100fb7a4, SetState
// 0x100fb934, ModelState 0x100fba44 and __sinit_Entertainer_cpp 0x100fbc84; and CStaff::~CStaff 0x100fb194 (which the inventory files here).
//
// An entertainer is a staff member: the Mac builds the CStaff part (and with it the CThing, the CDelayedScript at +0xc, the CPeep state and the
// Vehicle at +0xd4) with the CStaff constructor and adds a state word at +0x19c, the name at +0x1a0, the list link at +0x20c and the time the
// act started at +0x210. The host's CEntertainer is plain data at those offsets; the untranslated CStaff and CPeep members are called on it
// through casts of its address, as the other staff classes are.
//
// The constructor's two function statics (a guard byte and the costume counter) are kept as global variables, as is the pair of words
// __sinit_Entertainer_cpp stores (the host needs no construction function: both are constant initialised).

uint8_t gEntertainerCostumeGuard = 0;     // 0x1034c1d4
int32_t gEntertainerCostumeCounter = 0;   // 0x1034c1d8
uint32_t gEntertainerSinitLow = 0;        // 0x1034c1e0
uint32_t gEntertainerSinitHigh = 0x10000;  // 0x1034c1dc

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

// The walking speed: the halfword at 0x102a0d5a, the first entry (60) of a table of five halfwords {60, 80, 100, 120, 140}.
constexpr uint16_t kEntertainerSpeed = 60;
constexpr int32_t kStaffModel = 6;            // the model the CStaff constructor is given
constexpr int32_t kStateWaiting = 0;          // the entertainer stands and waits
constexpr int32_t kStateWalking = 1;          // walking to a patrol destination
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStateEntertaining = 0xe;
constexpr int32_t kFirstActScript = 0xd;       // the first of the custom act scripts
constexpr int32_t kNameCount = 0x23;           // text ids 0 to 0x22 are entertainer names
constexpr int32_t kNameCapacity = 0x20;
constexpr int32_t kTextResourceIndex = 13;     // gText + 0x580
constexpr uint32_t kThingModelPeep = 1;        // the model IsNearbyThingOfType is asked about
constexpr int32_t kSoundBankIndex = 4;         // gSoundBanks + 0x10
constexpr int32_t kSoundWaiting = 0xa5;
constexpr int32_t kSoundWalking = 0xa4;
constexpr int32_t kSoundActEnds = 0x87;

CPeep* AsPeep(CEntertainer* e) { return reinterpret_cast<CPeep*>(e); }
CStaff* AsStaff(CEntertainer* e) { return reinterpret_cast<CStaff*>(e); }

// The map cell the entertainer stands in: the 1 based cell id, built in 16 bits as the Mac does.
MapId CellOf(const CEntertainer* e) {
    return MapId{static_cast<uint16_t>(static_cast<uint16_t>(e->mThing.mCellX) + static_cast<uint16_t>(e->mThing.mCellY) * 0x80 + 1)};
}

}  // namespace

// 0x100fb094: a new entertainer; the costume is picked from a counter that runs 0, 1, 2, 0, ... The counter is set up on the first call only.
CEntertainer::CEntertainer(MapId cell) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel));
    if (gEntertainerCostumeGuard == 0) {
        gEntertainerCostumeCounter = 0;
        gEntertainerCostumeGuard = 1;
    }
    Init(static_cast<uint8_t>(gEntertainerCostumeCounter));
    gEntertainerCostumeCounter = static_cast<int32_t>(static_cast<uint32_t>(gEntertainerCostumeCounter) + 1u) % 3;   // the Mac's add wraps
}

// 0x100fb224: the entertainer the staff placement builds: `payGrade`, the costume and the text id of the name (0 to 0x22).
CEntertainer::CEntertainer(MapId cell, int32_t payGrade, uint8_t flag, int32_t value) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel), static_cast<CStaff::PayGrade>(payGrade), flag);
    GAME_ASSERTMSG(value >= 0 && value < kNameCount, "Name of entertainer out of bounds");
    static_cast<TbITable*>(gText.mResourcePointers[kTextResourceIndex])->FormatString(mName, kNameCapacity, static_cast<uint32_t>(value));
    Init(flag);
}

// 0x100fb194: the staff block's destructor. It destroys the Vehicle at +0xd4; the Mac's deleting form then frees the block (a plain
// `delete` of the host does that itself). The Mac skips everything when `this` is null.
CStaff::~CStaff() {
    reinterpret_cast<Vehicle*>(reinterpret_cast<uintptr_t>(this) + 0xd4)->~Vehicle();
}
// 0x100fb33c
void CEntertainer::Init(uint8_t costume) {
    mNext.mIndex = 0;
    gWorld->Register(this);
    mSpriteIndex = costume;
    mSpriteSet = 4;
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3), mSpriteSet, mSpriteIndex);
    mSpeed = kEntertainerSpeed;
    gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(0), CellOf(this));
    mTimeStartedEntertaining = 0;
    ChooseNextAction();
}

// 0x100fb448: the staff's own handling, unchanged.
void CEntertainer::ReceiveMessage(const CMsg& message) { AsStaff(this)->ReceiveMessage(message); }

// 0x100fb4a8: the staff block, then (for saves of version 0 and 1) the start time and the list link. Nonzero is a failure.
int32_t CEntertainer::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsStaff(this)->Serialise(file, save, version, unused) != 0) {
        return 1;
    }
    if (save == 0) {
        if (version < 2 && version >= 0) {
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mTimeStartedEntertaining), "mTimeStartedEntertaining", 0) != 0) {
                return 1;
            }
            if (LOADVARSTRUCT(file, &mNext, "mNext", 0) != 0) {
                return 1;
            }
        }
    } else if (version < 2 && version >= 0) {
        if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mTimeStartedEntertaining), "mTimeStartedEntertaining", 0) != 0) {
            return 1;
        }
        if (SAVEVARSTRUCT(file, &mNext, "mNext", 0) != 0) {
            return 1;
        }
    }
    return 0;
}

// 0x100fb600
void CEntertainer::FinalDestroy() {
    gWorld->Unregister(this);
    const MapId last = AsPeep(this)->LastCellVisited();
    gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(0), last);
    AsStaff(this)->FinalDestroy();
}

// 0x100fb6b0: the act's effect on the map follows the cell the entertainer last visited.
void CEntertainer::UpdateTurn() {
    const MapId last = AsPeep(this)->LastCellVisited();
    if (last.mIndex != CellOf(this).mIndex) {
        gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(0), last);
        gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(0), CellOf(this));
    }
    AsStaff(this)->UpdateTurn();
}

// 0x100fb7a4
void CEntertainer::ChooseNextAction() {
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    if (AsStaff(this)->TooTiredToWork() != 0) {
        CTRACE(2, "Too tired to entertain\n");
    } else {
        const uint32_t r = gWorld->GetRand();
        if (r % 3 == 0) {
            const MapId here = CellOf(this);
            CParkAnalyser* analyser = gWorld->GetParkAnalyser();
            if (analyser->IsNearbyThingOfType(static_cast<CThing::ThingModel>(kThingModelPeep), here,
                                              gMainBalance.mValues[kMainBalanceEntertainerActivationDistance + kMainBalanceStaffGradeStride * mGrade].mInteger) != 0) {
                SetState(static_cast<CStaff::StaffState>(kStateEntertaining));
                return;
            }
        }
    }
    if ((gWorld->mTurn & 3) == 0) {
        SetState(static_cast<CStaff::StaffState>(kStateWaiting));
    } else if (AsPeep(this)->SetRandomDestination() != 0) {
        SetState(static_cast<CStaff::StaffState>(kStateWalking));
    }
}

// 0x100fb934: the random number is drawn first, whichever state comes. An act picks one of the custom scripts at random.
void CEntertainer::SetState(CStaff::StaffState state) {
    const uint32_t r = gWorld->GetRand();
    if (state == kStateEntertaining) {
        const int32_t count = ESP_GetNumberOfCustomAnims(static_cast<ESP_Sprites>(mSpriteSet), mSpriteIndex);
        int32_t script;
        if (count == 1) {
            script = kFirstActScript;
        } else {
            script = static_cast<int32_t>(r % static_cast<uint32_t>(count - 1)) + kFirstActScript;
        }
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(script));
        mTimeStartedEntertaining = gWorld->mTurn;
    } else {
        AsStaff(this)->SetState(state);
    }
    mState = state;
}

// 0x100fba44
void CEntertainer::ModelState() {
    switch (mState) {
    case kStateResting:
        AsStaff(this)->ProcessResting();
        return;
    case kStateWalking: {
        if ((gWorld->GetRand() & 0xf) == 0) {
            AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundWalking);
        }
        const int32_t walks = AsPeep(this)->PeepWalks();
        if (walks != 0 && walks != 2) {
            AsStaff(this)->GetTiredPatrolling();
            return;
        }
        ChooseNextAction();
        return;
    }
    case kStateWalkingToRest:
        AsStaff(this)->ProcessWalkingToRestArea();
        return;
    case kStateWaiting:
        if ((gWorld->GetRand() & 0xf) == 0) {
            AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundWaiting);
        }
        if (gWorld->mTurn <= static_cast<uint32_t>(mStateTurn + gMainBalance.mValues[kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade].mInteger)) {
            return;
        }
        ChooseNextAction();
        return;
    case kStateEntertaining: {
        AsStaff(this)->GetTiredWorking();
        if (gWorld->mTurn <= static_cast<uint32_t>(mTimeStartedEntertaining + gMainBalance.mValues[kMainBalanceEntertainerWorkDuration + kMainBalanceStaffGradeStride * mGrade].mInteger)) {
            return;
        }
        float x, y, z;
        int32_t mode;
        SCR_GetScriptPosition(Script()->mSpriteScript, &x, &y, &z, &mode);
        SOUND_SendEvent(0, gSoundBanks[kSoundBankIndex], kSoundActEnds, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        ChooseNextAction();
        return;
    }
    default:
        AsStaff(this)->ModelState();
        return;
    }
}
