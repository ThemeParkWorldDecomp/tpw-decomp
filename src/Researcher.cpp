// CResearcher (main app, SimTheme Park 2.04b3, Researcher.cpp; unit w188): both constructors 0x10120e34 and 0x10120eec, Init 0x10121004,
// Serialise 0x101210e4, FinalDestroy 0x1012123c, ReceiveMessage 0x101212e4, UpdateTurn 0x10121340, ModelState 0x101213fc, ChooseNextAction
// 0x101214c0, the three Process states 0x101215d0 to 0x10121858, DoSomeResearch 0x101219cc and __sinit_Researcher_cpp 0x10121a88. The inventory
// also puts CRegionEffectCell's operator== 0x10120c7c and Serialise 0x10120cfc in this file.
//
// A researcher is a staff member (the Mac builds the CStaff part with the CStaff constructor) that does a little research for the research lab
// every twenty turns. It keeps the turn it started researching (+0x210) and the list link (+0x20c). The host's CResearcher is plain data at those
// offsets; the untranslated CStaff, CPeep and CResearchLab members are called on it through casts of the address, as the other staff classes do.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "Text.h"
#include "TbITable.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

// The two words the static initialiser stores (nothing reads either).
uint32_t gResearcherSinitLow = 0;
uint32_t gResearcherSinitHigh = 0x10000;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kStaffModel = 8;            // the model the CStaff constructor is given
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStateResearching = 15;
constexpr int32_t kNameCount = 0x23;
constexpr int32_t kNameCapacity = 0x20;
constexpr int32_t kTextResourceIndex = 15;    // gText + 0x588
constexpr int32_t kSoundBankIndex = 4;        // gSoundBanks + 0x10
constexpr int32_t kSoundPatrolling = 0xa8;
constexpr int32_t kSoundResearching = 0x8a;
constexpr int32_t kSoundIdle = 0xa9;
constexpr int32_t kSpriteScriptSpawn = 3;
constexpr int32_t kScriptResearching = 10;
constexpr int32_t kResearcherSpriteSet = 8;
constexpr uint32_t kResearchInterval = 20;    // turns between two bits of research

CPeep* AsPeep(CResearcher* r) { return reinterpret_cast<CPeep*>(r); }
CStaff* AsStaff(CResearcher* r) { return reinterpret_cast<CStaff*>(r); }

int32_t BalanceAt(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

// The Mac inlines the researcher's two SetState forms in each caller (there is no CResearcher::SetState): start researching ...
void StartResearching(CResearcher* r) {
    r->mTimeStartedResearching = gWorld->mTurn;
    r->Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptResearching));
    r->mState = kStateResearching;
}

// ... and start patrolling (the staff's SetState, then the state word).
void StartPatrolling(CResearcher* r) {
    AsStaff(r)->SetState(static_cast<CStaff::StaffState>(kStatePatrolling));
    r->mState = kStatePatrolling;
}

}  // namespace

// 0x10120c7c: the ten bytes are compared as they are.
bool CRegionEffectCell::operator==(const CRegionEffectCell& other) {
    return memcmp(this, &other, sizeof(CRegionEffectCell)) == 0;
}

// 0x10120cfc: five shorts, "mpEffect[i]". Nonzero is a failure. The Mac checks the version once before the loop (version 0 and 1).
int32_t CRegionEffectCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (save == 0) {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 5; i++) {
                if (LOADVAR(file, &mpEffect[i], "mpEffect[i]", 0) != 0) return 1;
            }
        }
    } else if (version < 2 && version >= 0) {
        for (int32_t i = 0; i < 5; i++) {
            if (SAVEVAR(file, &mpEffect[i], "mpEffect[i]", 0) != 0) return 1;
        }
    }
    return 0;
}

// 0x10120e34: the sprite index comes from ESP_GetRandomSpritesFrom.
CResearcher::CResearcher(MapId cell) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel));
    Init(static_cast<uint8_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(kResearcherSpriteSet))));
}

// 0x10120eec
CResearcher::CResearcher(MapId cell, int32_t payGrade, uint8_t flag, int32_t value) {
    new (this) CStaff(cell, static_cast<CThing::ThingModel>(kStaffModel), static_cast<CStaff::PayGrade>(payGrade), flag);
    GAME_ASSERTMSG(value >= 0 && value < kNameCount, "Name of researcher out of bounds");
    static_cast<TbITable*>(gText.mResourcePointers[kTextResourceIndex])->FormatString(mName, kNameCapacity, static_cast<uint32_t>(value));
    Init(flag);
}

// 0x10121004
void CResearcher::Init(uint8_t sprite) {
    mNext.mIndex = 0;
    gWorld->Register(this);
    mTimeStartedResearching = 0;
    mSpriteSet = kResearcherSpriteSet;
    mSpriteIndex = sprite;
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kSpriteScriptSpawn), mSpriteSet, mSpriteIndex);
    ChooseNextAction();
    gWorld->GetResearchLab()->UpdateActiveStatus();
}

// 0x101210e4: the staff block, then (for saves of version 0 and 1) the research turn and the list link. Nonzero is a failure.
int32_t CResearcher::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsStaff(this)->Serialise(file, save, version, unused) != 0) {
        return 1;
    }
    MacULong* started = reinterpret_cast<MacULong*>(&mTimeStartedResearching);
    if (save == 0) {
        if (version < 2 && version >= 0) {
            if (LOADVAR(file, started, "mTimeStartedResearching", 0) != 0) return 1;
            if (LOADVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (SAVEVAR(file, started, "mTimeStartedResearching", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mNext, "mNext", 0) != 0) return 1;
    }
    return 0;
}

// 0x1012123c: the lab is asked again afterwards (it may be gone).
void CResearcher::FinalDestroy() {
    gWorld->Unregister(this);
    AsStaff(this)->FinalDestroy();
    if (gWorld->GetResearchLab() != nullptr) {
        gWorld->GetResearchLab()->UpdateActiveStatus();
    }
}

// 0x101212e4
void CResearcher::ReceiveMessage(const CMsg& message) {
    AsStaff(this)->ReceiveMessage(message);
}

// 0x10121340: every twenty turns a researcher that is not resting does some research; the staff's own turn follows either way.
void CResearcher::UpdateTurn() {
    const int32_t general = AsStaff(this)->GetGeneralStaffState();
    if (general != 3 && general != 4 && general != 5) {
        if (gWorld->mTurn % kResearchInterval == 0) {
            DoSomeResearch();
        }
    }
    AsStaff(this)->UpdateTurn();
}

// 0x101213fc
void CResearcher::ModelState() {
    switch (static_cast<uint32_t>(mState)) {
    case kStateIdle: ProcessIdle(); return;
    case kStatePatrolling: ProcessPatrolling(); return;
    case kStateWalkingToRest: AsStaff(this)->ProcessWalkingToRestArea(); return;
    case kStateResting: AsStaff(this)->ProcessResting(); return;
    case kStateResearching: ProcessResearching(); return;
    default: AsStaff(this)->ModelState(); return;
    }
}

// 0x101214c0: patrol three times in four, else research unless too tired.
void CResearcher::ChooseNextAction() {
    if (AsStaff(this)->DoGeneralStaffThing() != 0) {
        return;
    }
    if ((gWorld->GetRand() & 3) != 0 && AsPeep(this)->SetRandomDestination() != 0) {
        StartPatrolling(this);
    } else if (AsStaff(this)->TooTiredToWork() != 0) {
        CTRACE(2, "Too tired to research\n");
    } else {
        StartResearching(this);
    }
}

// 0x101215d0 (the Mac inlines ChooseNextAction here)
void CResearcher::ProcessPatrolling() {
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

// 0x10121748: after the work duration of the grade the researcher patrols again (or starts the clock again if it cannot).
void CResearcher::ProcessResearching() {
    AsStaff(this)->GetTiredWorking();
    CWorld* world = gWorld;
    const uint32_t duration = static_cast<uint32_t>(BalanceAt(kMainBalanceResearcherWorkDuration + kMainBalanceResearcherGradeStride * mGrade));
    if (static_cast<uint32_t>(mTimeStartedResearching + duration) < world->mTurn) {
        if (AsPeep(this)->SetRandomDestination() != 0) {
            StartPatrolling(this);
        } else {
            mTimeStartedResearching = gWorld->mTurn;
        }
    } else if ((world->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundResearching);
    }
}

// 0x10121858 (the Mac inlines ChooseNextAction here)
void CResearcher::ProcessIdle() {
    if ((gWorld->GetRand() & 0xf) == 0) {
        AsPeep(this)->PlaySound(gSoundBanks[kSoundBankIndex], kSoundIdle);
    }
    const uint32_t idle = static_cast<uint32_t>(BalanceAt(kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mGrade));
    if (static_cast<uint32_t>(mStateTurn + idle) < gWorld->mTurn) {
        ChooseNextAction();
    }
}

// 0x101219cc
void CResearcher::DoSomeResearch() {
    const int32_t ability = BalanceAt(kMainBalanceResearchAbility + kMainBalanceResearcherGradeStride * mGrade);
    const ThingId id = mThing.GetThingId();
    CTRACE(3, "Researcher %d doing %d points of research\n", static_cast<uint32_t>(id.mIndex), ability);
    gWorld->GetResearchLab()->ContributeResearch(static_cast<uint32_t>(ability));
}
