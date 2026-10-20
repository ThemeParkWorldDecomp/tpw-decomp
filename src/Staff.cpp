// CStaff, part 1 (main app, SimTheme Park 2.04b3, staff.cpp; unit w189): both constructors 0x1012482c and 0x10124908, Init 0x10124a08, FinalDestroy
// 0x10124b88, ReceiveMessage 0x10124bd8, Serialise 0x10124ce8, UpdateTurn 0x10125208, SetState 0x101252a8, ModelState 0x101254f4, UpdateMonthly
// 0x10125598, the four enum serialise instances LOADVAR / SAVEVAR of CStaff::PayGrade and CStaff::StaffState (0x10127e38, 0x10127f70, 0x101280ac,
// 0x101281fc), CMsgStaffRoomEvent's destructor 0x1012622c and GetType 0x10127dfc, and __sinit_staff_cpp 0x10128350. Units w190 and w191 translate
// the rest of the class.
//
// A staff member is the base of the handyman, the guard, the entertainer, the mechanic and the researcher. The host's CStaff is plain data at the
// Mac's offsets (include/ThingReceivers.h); the CPeep part, the delayed script (+0xc) and the thoughts (+0x30) are reached through casts of the
// address, and the untranslated members are called as the other staff classes call them.
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

// The two words the static initialiser stores (nothing reads either; names invented).
uint32_t gStaffSinitLow = 0;
uint32_t gStaffSinitHigh = 0x10000;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kMsgRideDeleted = 10;       // the message the staff member is told a ride (its rest area) was deleted by
constexpr int32_t kMsgEndOfMonth = 12;        // the monthly message (UpdateMonthly)
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
constexpr uint16_t kSpeedBonusNone = 0;       // halfword 0 of the table at 0x102a0d54 ({0, 25, 50})
constexpr uint16_t kSpeedBonusChase = 25;     // halfword 1
constexpr uint16_t kWalkSpeedLow = 60;        // halfword 0 of the table at 0x102a0d5a ({60, 80, 100, 120, 140})
constexpr uint16_t kWalkSpeedHigh = 100;      // halfword 2
constexpr int32_t kWalkSpeedGrades = 3;       // the pay grades below it walk at the low speed
constexpr int32_t kScriptWalking = 9;
constexpr int32_t kScriptRest = 1;
constexpr int32_t kScriptState5 = 0x13;
constexpr int32_t kScriptState6 = 0x14;
constexpr int32_t kNameChars = 0x21;
constexpr int32_t kHistoryStaffWages = 3;     // the history whose mTemp totals the month's staff wages (index invented)

CPeep* AsPeep(CStaff* s) { return reinterpret_cast<CPeep*>(s); }

// A time stamp stored as two words: the second word first, as the Mac does.
void CopyStamp(uint32_t (&target)[2], const TbTimeStamp& stamp) {
    uint32_t words[2];
    std::memcpy(words, &stamp, sizeof words);
    target[1] = words[1];
    target[0] = words[0];
}

// The Mac makes the mouse and its default mode in this order inline (as src/Mouse_staff.cpp): the mouse (operator new(1)), then the default mode
// (operator new(8)), which the mouse takes; then the two words of the mouse's data are set to -1. A mouse that could not be made leaves the
// words alone. The answer is the mouse, NULL when it could not be made.
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

}  // namespace

// CStaff::PayGrade and CStaff::StaffState instances (the enums are four bytes: the same bodies as the int instances).
template<> int32_t LOADVAR<CStaff::PayGrade>(void* file, CStaff::PayGrade* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10127e38, 236 bytes
}
template<> int32_t LOADVAR<CStaff::StaffState>(void* file, CStaff::StaffState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10127f70, 236 bytes
}
template<> int32_t SAVEVAR<CStaff::PayGrade>(void* file, CStaff::PayGrade* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x101280ac, 260 bytes
}
template<> int32_t SAVEVAR<CStaff::StaffState>(void* file, CStaff::StaffState* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x101281fc, 260 bytes
}

// 0x1012482c: a staff member with no particulars (the pay grade is 0).
CStaff::CStaff(MapId cell, CThing::ThingModel model) {
    new (this) CPeep(cell, model);
    mTimeHired[1] = 0;
    mTimeHired[0] = 0;
    mHappiness = 0.0f;
    mEnergy = 0.0f;
    mCurrentPayGrade = 0;
    TRACE("Creating generic staff member (no particulars)\n");
    Init();
}

// 0x10124908: a staff member from the database; the costume is not used.
CStaff::CStaff(MapId cell, CThing::ThingModel model, PayGrade grade, uint8_t costume) {
    (void)costume;
    new (this) CPeep(cell, model);
    mCurrentPayGrade = grade;
    mTimeHired[1] = 0;
    mTimeHired[0] = 0;
    mHappiness = 0.0f;
    mEnergy = 0.0f;
    TRACE("Creating specific staff member from database\n");
    Init();
}

// 0x10124a08
void CStaff::Init() {
    mPercentageThroughGrade = 0;
    CopyStamp(mTimeHired, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    mRestArea.mIndex = 0;
    mTimeStartedIdling = 0;
    mJobsDone = 0;
    mTimeStartedIdling = 0;
    GAME_ASSERTMSG(1, "See what magic numbers do to you?  SORT IT OUT");
    mHappiness = 100.0f;
    mEnergy = 100.0f;
    CancelPatrolArea();
    mSpeedBonus = kSpeedBonusNone;
    if (mCurrentPayGrade < kWalkSpeedGrades) mWalkSpeed = kWalkSpeedLow;
    else mWalkSpeed = kWalkSpeedHigh;
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->Subscribe(mThing, static_cast<MsgType>(kMsgEndOfMonth));
    gWorld->GetParkAnalyser()->HiredSomeone();
}

// 0x10124b88
void CStaff::FinalDestroy() {
    AsPeep(this)->FinalDestroy();
}

// 0x10124bd8: the rest area was deleted (the staff member heads for another or stops resting there), or a month ended (the wages are paid).
void CStaff::ReceiveMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    if (type == kMsgRideDeleted) {
        const uint16_t deleted = static_cast<const CMsgRideDeleted&>(message).mThingId;
        if (deleted == mRestArea.mIndex) {
            if (mState == kStateResting) {
                StopResting();
                GoToRestArea();
            } else if (mState == kStateWalkingToRest) {
                mRestArea.mIndex = 0;
                SetState(static_cast<StaffState>(kStateIdle));
            }
        }
    } else if (type == kMsgEndOfMonth) {
        UpdateMonthly();
    }
}

// 0x10124ce8: version 0 or 1 (nothing else is read or written). The happiness and the energy are saved as a whole number from 0 to 255 (a float
// "pv"); the time hired is handed over as the stamp in place.
int32_t CStaff::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (AsPeep(this)->Serialise(file, save, version, unused) != 0) return 1;
    if (save != 0) {
        if (version >= 2 || version < 0) return 0;
        if (SAVEVAR(file, reinterpret_cast<PayGrade*>(&mCurrentPayGrade), "mCurrentPayGrade", 0) != 0) return 1;
        float pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mHappiness)));
        if (SAVEVAR(file, &pv, "pv", 0) != 0) return 1;
        if (SAVEVAR(file, &mJobsDone, "mJobsDone", 0) != 0) return 1;
        for (int32_t i = 0; i < kNameChars; ++i) {
            if (SAVEVAR(file, &mName[i], "mName[i]", 0) != 0) return 1;
        }
        if (SAVEVARSTRUCT(file, &mPatrolRegionBL, "mPatrolRegionBL", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mPatrolRegionTR, "mPatrolRegionTR", 0) != 0) return 1;
        if (SAVEVAR(file, &mPercentageThroughGrade, "mPercentageThroughGrade", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mRestArea, "mRestArea", 0) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<StaffState*>(&mState), "mState", 0) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mTimeStartedIdling), "mTimeStartedIdling", 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, reinterpret_cast<TbTimeStamp*>(mTimeHired), "mTimeHired", 0) != 0) return 1;
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mEnergy)));
        if (SAVEVAR(file, &pv, "pv", 0) != 0) return 1;
    } else {
        if (version >= 2 || version < 0) return 0;
        if (LOADVAR(file, reinterpret_cast<PayGrade*>(&mCurrentPayGrade), "mCurrentPayGrade", 0) != 0) return 1;
        float pv = 0.0f;
        if (LOADVAR(file, &pv, "pv", 0) != 0) return 1;
        mHappiness = pv;
        if (LOADVAR(file, &mJobsDone, "mJobsDone", 0) != 0) return 1;
        for (int32_t i = 0; i < kNameChars; ++i) {
            if (LOADVAR(file, &mName[i], "mName[i]", 0) != 0) return 1;
        }
        if (LOADVARSTRUCT(file, &mPatrolRegionBL, "mPatrolRegionBL", 0) != 0) return 1;
        if (LOADVARSTRUCT(file, &mPatrolRegionTR, "mPatrolRegionTR", 0) != 0) return 1;
        if (LOADVAR(file, &mPercentageThroughGrade, "mPercentageThroughGrade", 0) != 0) return 1;
        if (LOADVARSTRUCT(file, &mRestArea, "mRestArea", 0) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<StaffState*>(&mState), "mState", 0) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<MacULong*>(&mTimeStartedIdling), "mTimeStartedIdling", 0) != 0) return 1;
        if (LOADVARSTRUCT(file, reinterpret_cast<TbTimeStamp*>(mTimeHired), "mTimeHired", 0) != 0) return 1;
        if (LOADVAR(file, &pv, "pv", 0) != 0) return 1;
        mEnergy = pv;
    }
    return 0;
}

// 0x10125208: the thought bubbles are processed every fourth turn, the staff members taking turns by their thing id.
void CStaff::UpdateTurn() {
    AsPeep(this)->UpdateTurn();
    const uint16_t id = mThing.GetThingId().mIndex;
    const uint32_t turn = gWorld->mTurn;
    if ((turn & 3u) == (static_cast<uint32_t>(id) & 3u)) Thoughts()->ProcessThoughtBubbles();
}

// 0x101252a8
void CStaff::SetState(StaffState state) {
    switch (static_cast<uint32_t>(state)) {
    case 0:
        if (mState == kStatePatrolling) mTimeStartedIdling = gWorld->mTurn;
        else mTimeStartedIdling = 0;
        mSpeedBonus = kSpeedBonusNone;
        AsPeep(this)->SetStandScript();
        break;
    case 1:
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptWalking));
        break;
    case 2:
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptRest));
        break;
    case 3:
        Thoughts()->RemoveThoughtBubble();
        Script()->KillSpriteScript();
        break;
    case 4:
        mSpeedBonus = kSpeedBonusChase;
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptWalking));
        break;
    case 5:
        mF1C = 0;
        mSpeedBonus = kSpeedBonusNone;
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptState5));
        break;
    case 6:
        mTimeStartedIdling = gWorld->mTurn;
        Script()->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptState6));
        break;
    case 7: {   // picked up by the player
        Script()->KillSpriteScript();
        Thoughts()->RemoveThoughtBubble();
        CMMPickUpStaff* mode = new (std::nothrow) CMMPickUpStaff(static_cast<ESP_Sprites>(mSpriteSet), static_cast<uint8_t>(mSpriteIndex));
        if (gMouse == nullptr) gMouse = MakeMouse();
        gMouse->AssignMouseMode(mode);
        break;
    }
    default:
        GAME_ASSERTMSG(0, "Unknown state in CStaff::SetState");
        break;
    }
    mState = static_cast<int32_t>(state);
}

// 0x101254f4
void CStaff::ModelState() {
    switch (mState) {
    case 4: ProcessWalkingToStrike(); break;
    case 5: ProcessStriking(); break;
    case 6: ProcessBeingStunned(); break;
    case 7: break;
    default: GAME_ASSERTMSG(0, "Unknown state in CStaff::ModelState"); break;
    }
}

// 0x10125598: the month's wages are paid out of the bank account and added to the analyser's staff wages total.
void CStaff::UpdateMonthly() {
    CWorld* world = gWorld;
    const uint32_t salary = GetSalary();
    world->GetBankAccount()->MakeWithdrawal(salary);
    world = gWorld;
    const uint32_t again = GetSalary();
    CParkAnalyser* analyser = world->GetParkAnalyser();
    analyser->mHistory1F100[kHistoryStaffWages].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[kHistoryStaffWages].mTemp) + again);
}

// 0x1012622c (the deleting form: the vtable is reset and the block freed for a positive flag, which the delete expression does).
CMsgStaffRoomEvent::~CMsgStaffRoomEvent() {}

// 0x10127dfc
int32_t CMsgStaffRoomEvent::GetType() const { return 15; }
