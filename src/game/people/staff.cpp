// staff.cpp: name proven by __sinit_staff_cpp in the main application.

#include "analyser.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "ppc_math.h"
#include "rsse.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// CStaff, part 1 (main app, SimTheme Park 2.04b3, staff.cpp; unit w189): both constructors 0x1012482c and 0x10124908, Init 0x10124a08, FinalDestroy
// 0x10124b88, ReceiveMessage 0x10124bd8, Serialise 0x10124ce8, UpdateTurn 0x10125208, SetState 0x101252a8, ModelState 0x101254f4, UpdateMonthly
// 0x10125598, the four enum serialise instances LOADVAR / SAVEVAR of CStaff::PayGrade and CStaff::StaffState (0x10127e38, 0x10127f70, 0x101280ac,
// 0x101281fc), CMsgStaffRoomEvent's destructor 0x1012622c and GetType 0x10127dfc, and __sinit_staff_cpp 0x10128350. Units w190 and w191 translate
// the rest of the class.
//
// A staff member is the base of the handyman, the guard, the entertainer, the mechanic and the researcher. The host's CStaff is plain data at the
// Mac's offsets (include/ThingReceivers.h); the CPeep part, the delayed script (+0xc) and the thoughts (+0x30) are reached through casts of the
// address, and the untranslated members are called as the other staff classes call them.

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

// CStaff, part 2 (main app, SimTheme Park 2.04b3, staff.cpp; unit w190): SackMe 0x10125660, TrainMe 0x10125854, GetEmploymentDuration 0x10125a10,
// CanBePickedUp 0x10125aec, PickMeUp 0x10125b98, DropMe 0x10125de4, BeingPointedAt 0x10125f30, GetStaffType 0x10126718, GetGeneralStaffState
// 0x101267cc, GetSkill 0x1012686c, IsStriking 0x10126904, IsAllocatedToRide 0x101269d8, IsWorkingOnAllocatedObject 0x10126aac,
// IsRestingInStaffRoom 0x10126c1c, TooTiredToWork 0x10126cb4, SetName 0x10126f30, GetName 0x10126f98 and the two GetSalary (0x10126fc8 and the
// static one 0x101270b0). Part 1 is src/Staff.cpp (unit w189).
//
// The kind byte of the thing (+2) names the staff class: 5 handyman, 4 mechanic, 6 entertainer, 7 guard, 8 researcher. The staff type the
// pay tables and the staff headquarters use is another numbering (0 handyman, 1 mechanic, 2 entertainer, 3 guard, 4 researcher).

namespace {

constexpr int64_t kTicksPerDay = 864000000000LL;
constexpr int32_t kKindMechanic = 4;
constexpr int32_t kKindHandyman = 5;
constexpr int32_t kKindEntertainer = 6;
constexpr int32_t kKindGuard = 7;
constexpr int32_t kKindResearcher = 8;
constexpr int32_t kStatePickedUp = 7;
constexpr int32_t kStateCleaningToilet = 0xb;
constexpr int32_t kStateRepairing = 0xd;
constexpr int32_t kStateBeingStunned = 0x12;
constexpr int32_t kMouseModeWalking = 6;              // CMouseMode::GetMode answer of the mode a picked up staff member leaves
constexpr int32_t kHistoryTraining = 4;               // the history whose mTemp totals the month's training costs (index invented)
constexpr int32_t kParticleSacked = 0x13;
constexpr int32_t kMaxTrainingPoints = 100;
constexpr uint32_t kTraceLevelDefault = 2;
constexpr uint32_t kTraceLevelTraining = 0x17;
constexpr uint32_t kTraceLevelPromotion = 0x75bcd15;   // the level register is built from lis 0x75c and -0x32eb

// The text pool at 0x10245808 (the same pool as src/Staff.cpp's texts): the strings of this unit.
const char kTrainFull[] = "Trying to train already fully-trained staff member\n";   // 0x10245969
const char kTrainUnknown[] = "Unknown staff type in CStaff::TrainMe\n";               // 0x1024599d
const char kPromoted[] = "Staff type %d got promoted!!!\n";                            // 0x102459c4
const char kCannotPickUp[] = "Trying to pick up staff member who cannot be picked up! (cos he's in state %d)\n";   // 0x102459e3
const char kPickingUp[] = "Picking up staff member %d\n";                             // 0x10245a33
const char kDropping[] = "Dropping staff member %d\n";                                // 0x10245a4f
const char kInvalidType[] = "Invalid staff type\n";                                    // 0x10245b30
const char kInvalidCall[] = "Invalid call\n";                                          // 0x10245b44

// divw: the quotient truncated towards zero; the dividend where the quotient is undefined (the emulator's answer, as in src/ui_mainpanel2_create.cpp).
inline int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// SackMe and PickMeUp open alike: the mouse is made when there is none, and a mouse still in the mode with the answer 6 gets a new default mode.
void LeaveWalkingMouseMode() {
    if (gMouse == nullptr) gMouse = MakeMouse();
    if (gMouseMode != nullptr && gMouseMode->GetMode() == kMouseModeWalking) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        if (gMouse == nullptr) gMouse = MakeMouse();
        gMouse->AssignMouseMode(mode);
    }
}

// The kind byte to the staff type; any other kind asserts and answers 5 (a doubt: the register the listing leaves it in).
StaffType StaffTypeOfKind(uint8_t kind) {
    switch (kind) {
    case kKindHandyman: return static_cast<StaffType>(0);
    case kKindMechanic: return static_cast<StaffType>(1);
    case kKindEntertainer: return static_cast<StaffType>(2);
    case kKindGuard: return static_cast<StaffType>(3);
    case kKindResearcher: return static_cast<StaffType>(4);
    default: GAME_ASSERTMSG(0, kInvalidType); return static_cast<StaffType>(5);
    }
}
}  // namespace

// 0x10125660: dismissed: the mouse leaves its pick up mode, the last wage is paid, the staff member goes up in smoke.
void CStaff::SackMe() {
    LeaveWalkingMouseMode();
    CWorld* world = gWorld;
    const uint32_t salary = GetSalary();
    world->GetBankAccount()->MakeWithdrawal(salary);
    world = gWorld;
    const uint32_t again = GetSalary();
    CParkAnalyser* analyser = world->GetParkAnalyser();
    analyser->mHistory1F100[kHistoryStaffWages].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[kHistoryStaffWages].mTemp) + again);
    reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(kParticleSacked);
    mThing.KillMe();
}

// 0x10125854: the amount is paid and gives training points at the pounds a point of his class and grade; 100 points are a grade.
void CStaff::TrainMe(int32_t amount) {
    if (mCurrentPayGrade == 4) {
        CTRACE(kTraceLevelTraining, kTrainFull);
        return;
    }
    gWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(amount));
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    analyser->mHistory1F100[kHistoryTraining].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[kHistoryTraining].mTemp) + static_cast<uint32_t>(amount));
    int32_t perPoint;
    switch (mThing.mKind) {
    case kKindEntertainer: perPoint = gMainBalance.mValues[kMainBalanceEntertainerPoundsPerTrainingPoint + kMainBalanceEntertainerGradeStride * mCurrentPayGrade].mInteger; break;
    case kKindHandyman: perPoint = gMainBalance.mValues[kMainBalanceHandymanPoundsPerTrainingPoint + kMainBalanceHandymanGradeStride * mCurrentPayGrade].mInteger; break;
    case kKindGuard: perPoint = gMainBalance.mValues[kMainBalanceGuardPoundsPerTrainingPoint + kMainBalanceGuardGradeStride * mCurrentPayGrade].mInteger; break;
    case kKindMechanic: perPoint = gMainBalance.mValues[kMainBalanceMechanicPoundsPerTrainingPoint + kMainBalanceMechanicGradeStride * mCurrentPayGrade].mInteger; break;
    case kKindResearcher: perPoint = gMainBalance.mValues[kMainBalanceResearcherPoundsPerTrainingPoint + kMainBalanceResearcherGradeStride * mCurrentPayGrade].mInteger; break;
    default:
        GAME_ASSERTMSG(0, kTrainUnknown);
        perPoint = 1;   // the Mac divides by a register nothing set here (the caller's r26): not reproducible
        break;
    }
    int32_t points = Div(amount, perPoint);
    if (points > kMaxTrainingPoints) points = kMaxTrainingPoints;
    const int32_t total = static_cast<int32_t>(mPercentageThroughGrade) + points;
    if (total > 99) {
        CTRACE(kTraceLevelPromotion, kPromoted, static_cast<int32_t>(mThing.mKind));
        mCurrentPayGrade = mCurrentPayGrade + 1;
        mPercentageThroughGrade = static_cast<uint8_t>(mPercentageThroughGrade + (points - kMaxTrainingPoints));
        mHappiness = 100.0f;
    } else {
        mPercentageThroughGrade = static_cast<uint8_t>(total);
    }
}

// 0x10125a10: whole days since he was hired (the span of the park's funny time is divided by a day; the low word is kept).
int32_t CStaff::GetEmploymentDuration() const {
    static_assert(sizeof(TbTimeStamp) == sizeof(mTimeHired), "a time stamp is two words");
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    TbTimeStamp now = savvy->GetWorld().mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    TbTimeDiff span = now - *reinterpret_cast<const TbTimeStamp*>(mTimeHired);
    return static_cast<int32_t>(span.mTicks / kTicksPerDay);
}

// 0x10125aec: not struck (general state 4 or 5), not in state 0xd or 0x12 and inside the park.
int32_t CStaff::CanBePickedUp() {
    if (GetGeneralStaffState() != 4 && GetGeneralStaffState() != 5 && mState != kStateRepairing && mState != kStateBeingStunned &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
        return 1;
    }
    return 0;
}

// 0x10125b98: the mouse carries him; the staff headquarters is told, then he is in state 7.
void CStaff::PickMeUp() {
    int32_t ok = 0;
    if (GetGeneralStaffState() != 4 && GetGeneralStaffState() != 5 && mState != kStateRepairing && mState != kStateBeingStunned &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
        ok = 1;
    }
    GAME_ASSERTMSG(ok, kCannotPickUp, mState);
    const uint16_t id = mThing.GetThingId().mIndex;
    CTRACE(kTraceLevelDefault, kPickingUp, id);
    LeaveWalkingMouseMode();
    CWorld* world = gWorld;
    const ThingId again = mThing.GetThingId();
    world->GetStaffHQ()->PickUpStaff(again);
    SetState(static_cast<StaffState>(kStatePickedUp));
}

// 0x10125de4: put down in the (1 based) cell; the mechanic is given his jobs again, the others go idle.
void CStaff::DropMe(MapId cell) {
    CTRACE(kTraceLevelDefault, kDropping, static_cast<uint16_t>(mThing.GetThingId().mIndex));
    const uint32_t c = static_cast<uint32_t>(cell.mIndex) - 1;
    const MapPos x = {static_cast<uint8_t>(c & 0x7f), 0x80};
    const MapPos y = {static_cast<uint8_t>((c >> 7) & 0xff), 0x80};
    reinterpret_cast<CPeep*>(this)->Teleport(x, y);
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3), mSpriteSet, mSpriteIndex);
    if (mThing.mKind == kKindMechanic) {
        CMechanic* mechanic = reinterpret_cast<CMechanic*>(this);
        gWorld->GetMechanicHQ()->ReassignJobsAfterDrop(*mechanic, cell);
        mechanic->ChooseNextAction();
    } else {
        SetState(static_cast<StaffState>(kStateIdle));
        mTimeStartedIdling = 0;
    }
}

// 0x10125f30: the mouse is over him; a guard in state 0x12 stays as he is.
void CStaff::BeingPointedAt() {
    int32_t ok = 0;
    if (GetGeneralStaffState() != 4 && GetGeneralStaffState() != 5 && mState != kStateRepairing && mState != kStateBeingStunned &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
        ok = 1;
    }
    if (ok != 0 && (mThing.mKind != kKindGuard || mState != kStateBeingStunned)) {
        SetState(static_cast<StaffState>(kStateIdle));
    }
}

// CStaff, part 3 (main app, SimTheme Park 2.04b3, staff.cpp; unit w191): ProcessWalkingToRestArea 0x10126008, ProcessResting 0x101262a8,
// ProcessWalkingToStrike 0x10126444, ProcessStriking 0x10126504, ProcessBeingStunned 0x10126688, GetTiredPatrolling 0x10126d1c, GetTiredWorking
// 0x10126e28, FindNearestRestingPlace 0x10127108, DoGeneralStaffThing 0x101272b0, GoToRestArea 0x10127658, StopResting 0x10127764, SetPatrolArea
// 0x101278f0, GetPatrolArea 0x1012798c, CancelPatrolArea 0x10127a24, HasPatrolArea 0x10127a78, InsidePatrolArea 0x10127af8 and FindDestInPatrolArea
// 0x10127c10. Part 1 is src/Staff.cpp (unit w189) and part 2 src/Staff_queries.cpp (unit w190).
//
// A staff member rests in a room (an object that chills you out): it walks to the nearest one, takes a place there (the room's script variable 0
// counts the people inside) and recovers energy and happiness until the energy reads 100, when the class's ChooseNextAction takes over. Working and
// patrolling cost energy and happiness (GetTired*). A tired staff member with no room, or an unhappy one, may go on strike (DoGeneralStaffThing) and
// walks to the strike area; the patrol area is a box of map cells (bottom left and top right) the handyman and the guard keep to. The host's CStaff
// is plain data at the Mac's offsets (include/ThingReceivers.h); the untranslated and the other units' members are called as the other staff classes
// call them.

namespace {

constexpr int32_t kStateWalkingToStrike = 4;
constexpr int32_t kStateStriking = 5;
constexpr int32_t kPeepArrived = 0;
constexpr int32_t kPeepNoWay = 2;
constexpr int32_t kThoughtWalkingToStrike = 0x15;
constexpr int32_t kThoughtTired = 0x14;
constexpr int32_t kThoughtUnhappy = 0x13;
constexpr int32_t kThoughtHappy = 0x12;
constexpr int32_t kThoughtNoPatrolDest = 0x16;
constexpr int32_t kScriptStanding = 3;
constexpr uint32_t kTraceLevelStrike = 0xc;
constexpr uint32_t kMapCellSize = 0x44;
constexpr uint32_t kNullThing = 0;             // the words at 0x102a1174, 0x102a1176 and 0x102a1178
constexpr uint16_t kPatrolBottomLeftNone = 1;  // CancelPatrolArea: no patrol area
constexpr uint16_t kPatrolTopRightNone = 0x4000;
constexpr float kMaxStat = 100.0f;             // 0x10265320
constexpr float kMinStat = 0.0f;               // 0x10265324
constexpr uint32_t kMaxStrikeTimer = 0x7ff;
constexpr int32_t kPatrolTries = 30;

// The walking speeds: the halfwords from 0x102a0d5a ({60, 80, 100, 120, 140}); DoGeneralStaffThing picks one by the energy / 20.
const uint16_t kWalkSpeeds[5] = {60, 80, 100, 120, 140};

// The text pool at 0x10245808 (the same pool as src/Staff.cpp's texts): the strings of this unit.
const char kRowAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";            // 0x10245a69
const char kNoRestAreaWay[] = "Staff member couldn't get to his rest area\n";                                 // 0x10245aac
const char kUnknownType[] = "Unknown staff type in CStaff::ProcessResting\n";                                 // 0x10245ad8
const char kNoEntrance[] = "Staff member couldn't find park entrance\n";                                      // 0x10245b06
const char kLookingForRest[] = "Looking for rest area...\n";                                                  // 0x10245b52
const char kStrike[] = "Right!  I'm fed up, I'm going on strike!  And I'm going to that bus stop!\n";      // 0x10245b6c
const char kStrikeNoWay[] = "*** Could not get to strike location ***\n";                                   // 0x10245bb7
const char kNoRestArea[] = "Staff member couldn't find a rest area\n";                                       // 0x10245be1
const char kFoundRest[] = "Found a rest area!";                                                              // 0x10245c09
const char kNoWayToRest[] = "Staff: couldn't get to nearest rest area...\n";                                // 0x10245c1c
const char kNoPatrolDest[] = "Could not find or reach a destination in the patrol area\n";                  // 0x10245c49

MapId CellOfThing(const CThing& t) {
    return MapId{static_cast<uint16_t>(static_cast<uint16_t>(t.mCellX) + static_cast<uint16_t>(t.mCellY) * 0x80 + 1)};
}

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

// The x and y a cell id has on the map (cell ids start at 1; 32-bit unsigned, so cell 0 gives x 0x7f and y 0x1ffffff).
uint32_t CellX(MapId id) { return (static_cast<uint32_t>(id.mIndex) - 1u) & 0x7fu; }
uint32_t CellY(MapId id) { return (static_cast<uint32_t>(id.mIndex) - 1u) >> 7; }

// divwu and the multiply-subtract after it: the remainder; a zero divisor gives the dividend (the emulator's divwu answers 0).
uint32_t UnsignedRemainder(uint32_t a, uint32_t b) {
    const uint32_t quotient = b == 0 ? 0u : a / b;
    return a - quotient * b;
}

// (float)(int32_t): the 0x4330 xoris trick then fsubs.
float IntToFloat(int32_t value) { return static_cast<float>(value); }

// The five clamps of the energy and the happiness are one pattern: t = value - d (fneg then fadds); above 100 the stat becomes 100, below 0 it becomes 0,
// otherwise value - d (fsubs; the same number). A NaN t falls through to the last store (ble and bge are "not greater" and "not less").
float ClampedMinus(float value, float d) {
    const float t = -d + value;
    if (t > kMaxStat) return kMaxStat;
    if (t < kMinStat) return kMinStat;
    return value - d;
}

// ProcessResting's clamps add: the stat becomes t itself.
float ClampedPlus(float rate, float value) {
    const float t = rate + value;
    if (t > kMaxStat) return kMaxStat;
    if (t < kMinStat) return kMinStat;
    return t;
}

float BalanceFloatAt(int32_t slot) { return gMainBalance.mValues[slot].mFloat; }
int32_t BalanceAt(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

// The message centre is made on first use (the Mac's operator new(0x1d0) and __construct_array of 0x1d headers), as src/Guard.cpp does.
void PostRoomEvent(const CMsgStaffRoomEvent& message) {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->PostMessage(message);
}

}  // namespace

// 0x10126008: walks to the rest area; on arrival takes a place in the room (the room's script variable 0 counts the people inside), becomes
// resting and tells the message centre; with no way there the staff member loses happiness and goes idle.
void CStaff::ProcessWalkingToRestArea() {
    const int32_t result = AsPeep(this)->PeepWalks();
    if (result == kPeepArrived) {
        CObject* room = ObjectOf(RowOf(mRestArea));
        RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(room->mF20));
        RSSE_Setvar(script, 0, RSSE_Getvar(script, 0) + 1);
        SetState(static_cast<StaffState>(kStateResting));
        const ThingId id = mThing.GetThingId();
        const CMsgStaffRoomEvent message(id.mIndex, 1);
        PostRoomEvent(message);
    } else if (result == kPeepNoWay) {
        CTRACE(kTraceLevelDefault, kNoRestAreaWay);
        mHappiness = ClampedMinus(mHappiness, IntToFloat(BalanceAt(kMainBalanceHappyHitCosNoRestArea)));
        SetState(static_cast<StaffState>(kStateIdle));
    }
}

// 0x1012622c (the deleting form: the vtable is reset and the block freed for a positive flag, which the delete expression does).
CMsgStaffRoomEvent::~CMsgStaffRoomEvent() {}

// 0x101262a8: one turn of rest: the energy and the happiness recover by the grade's rates (clamped to 0..100); at an energy of 100 (the low byte
// of the truncated value) the staff member leaves the room and the class's ChooseNextAction takes over.
void CStaff::ProcessResting() {
    mEnergy = ClampedPlus(BalanceFloatAt(kMainBalanceStaffRecuperationRate + kMainBalanceStaffGradeStride * mCurrentPayGrade), mEnergy);
    mHappiness = ClampedPlus(BalanceFloatAt(kMainBalanceStaffHappinessRecuperationRate + kMainBalanceStaffGradeStride * mCurrentPayGrade), mHappiness);
    if (static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(mEnergy))) == 100) {
        StopResting();
        switch (mThing.mKind) {
        case kKindEntertainer: reinterpret_cast<CEntertainer*>(this)->ChooseNextAction(); break;
        case kKindHandyman: reinterpret_cast<CHandyman*>(this)->ChooseNextAction(); break;
        case kKindMechanic: reinterpret_cast<CMechanic*>(this)->ChooseNextAction(); break;
        case kKindGuard: reinterpret_cast<CGuard*>(this)->ChooseNextAction(); break;
        case kKindResearcher: reinterpret_cast<CResearcher*>(this)->ChooseNextAction(); break;
        default: GAME_ASSERTMSG(0, kUnknownType); break;
        }
    }
}

// 0x10126444: the walk to the strike area; the bubble is shown every turn, and the walk ending (arrived or no way) starts the strike.
void CStaff::ProcessWalkingToStrike() {
    const int32_t result = AsPeep(this)->PeepWalks();
    Thoughts()->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(kThoughtWalkingToStrike), false);
    if (result == kPeepArrived || result == kPeepNoWay) {
        mF188 = 0;
        SetState(static_cast<StaffState>(kStateStriking));
    }
}

// 0x10126504: the strike. One turn in eight the timer (mF1C) moves by a random byte minus 0x80, capped at 0x7ff (the compare is unsigned: a negative
// sum becomes 0x7ff). A closed, empty park ends the strike; a staff member whose strike has ended walks to the entrance.
void CStaff::ProcessStriking() {
    const uint32_t rand = gWorld->GetRand();
    if ((rand & 7u) == 0) {
        uint32_t timer = (rand & 0xffu) + static_cast<uint32_t>(mF1C) - 0x80u;
        if (timer > kMaxStrikeTimer) timer = kMaxStrikeTimer;
        mF1C = static_cast<int32_t>(timer);
    }
    if (gWorld->IsParkClosed() != 0) {
        if (gWorld->GetParkAnalyser()->NumberOfVisitorsInPark() == 0) {
            CWorld* world = gWorld;
            const StaffType type = GetStaffType();
            world->GetStaffHQ()->StopStrikeInProgress(type);
        }
    }
    if (IsStriking() == 0) {
        const uint16_t cell = static_cast<uint16_t>(static_cast<uint16_t>(BalanceAt(kMainBalanceEntranceAPosX)) +
                                                    static_cast<uint16_t>(static_cast<uint32_t>(BalanceAt(kMainBalanceEntranceAPosY)) << 7) + 1);
        if (AsPeep(this)->SetDest(MapId{cell}) != 0) {
            SetState(static_cast<StaffState>(kStatePatrolling));
        } else {
            CTRACE(kTraceLevelDefault, kNoEntrance);
            SetState(static_cast<StaffState>(kStateIdle));
        }
    }
}

// 0x10126688: a stunned staff member is back to idle once the time since mTimeStartedIdling exceeds three times the grade's idle duration (unsigned).
void CStaff::ProcessBeingStunned() {
    const uint32_t limit = static_cast<uint32_t>(BalanceAt(kMainBalanceStaffIdleDuration + kMainBalanceStaffGradeStride * mCurrentPayGrade)) * 3u;
    if (limit < gWorld->mTurn - mTimeStartedIdling) SetState(static_cast<StaffState>(kStateIdle));
}

// 0x10126718
StaffType CStaff::GetStaffType() {
    return StaffTypeOfKind(mThing.mKind);
}

// 0x101267cc: the staff member's state folded to a general one (0 idle, 1 patrolling, 2 any other, 3 resting or walking to rest, 4 the two
// struck states, 5 picked up).
int32_t CStaff::GetGeneralStaffState() const {
    switch (mState) {
    case 0: return 0;
    case 1: return 1;
    case 2:
    case 3: return 3;
    case 4:
    case 5: return 4;
    case 7: return 5;
    default: return 2;
    }
}

// 0x1012686c: (grade + percentage / 100) * 20, as an unsigned word (CodeWarrior's double to unsigned conversion). The grade is made a single
// first (fsubs), the percentage a double.
int32_t CStaff::GetSkill() const {
    const float grade = static_cast<float>(mCurrentPayGrade);
    const double sum = static_cast<double>(grade) + static_cast<double>(mPercentageThroughGrade) / 100.0;
    const float scaled = 20.0f * static_cast<float>(sum);
    return static_cast<int32_t>(cw_cvt_fp2unsigned(static_cast<double>(scaled)));
}

// 0x10126904: whether his staff type is on strike.
int32_t CStaff::IsStriking() {
    const StaffType type = StaffTypeOfKind(mThing.mKind);
    return gWorld->GetStaffHQ()->IsStriking(type);
}

// 0x101269d8: the mechanic's ride to repair or the handyman's toilet to clean is this thing.
int32_t CStaff::IsAllocatedToRide(ThingId ride) {
    if (mThing.mKind == kKindMechanic) {
        return ride.mIndex == reinterpret_cast<const CMechanic*>(this)->mObjectToRepair.mIndex ? 1 : 0;
    }
    if (mThing.mKind == kKindHandyman) {
        return ride.mIndex == reinterpret_cast<const CHandyman*>(this)->mToiletToClean.mIndex ? 1 : 0;
    }
    GAME_ASSERTMSG(0, kInvalidCall);
    return 0;
}

// 0x10126aac: allocated to it and busy with it (the handyman cleaning the toilet, the mechanic repairing).
int32_t CStaff::IsWorkingOnAllocatedObject(ThingId object) {
    int32_t allocated;
    if (mThing.mKind == kKindMechanic) {
        allocated = object.mIndex == reinterpret_cast<const CMechanic*>(this)->mObjectToRepair.mIndex ? 1 : 0;
    } else if (mThing.mKind == kKindHandyman) {
        allocated = object.mIndex == reinterpret_cast<const CHandyman*>(this)->mToiletToClean.mIndex ? 1 : 0;
    } else {
        GAME_ASSERTMSG(0, kInvalidCall);
        allocated = 0;
    }
    if (allocated == 0) return 0;
    if (mThing.mKind == kKindHandyman && mState == kStateCleaningToilet) return 1;
    if (mThing.mKind == kKindMechanic && mState == kStateRepairing) return 1;
    return 0;
}

// 0x10126c1c: resting, and the room is his rest area.
int32_t CStaff::IsRestingInStaffRoom(ThingId room) const {
    return (mState == kStateResting && room.mIndex == mRestArea.mIndex) ? 1 : 0;
}

// 0x10126cb4: his energy (whole points, as a byte) is below the rest level of the balance table.
int32_t CStaff::TooTiredToWork() const {
    return static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(mEnergy))) < gMainBalance.mValues[kMainBalanceRestLevel].mInteger ? 1 : 0;
}

// 0x10126d1c: patrolling costs 0.012 energy and 0.005 happiness a turn for each grade below 6 (the products are doubles, rounded to single).
void CStaff::GetTiredPatrolling() {
    const double energyCost = 0.012 * static_cast<double>(IntToFloat(static_cast<int32_t>(6u - static_cast<uint32_t>(mCurrentPayGrade))));
    mEnergy = ClampedMinus(mEnergy, static_cast<float>(energyCost));
    const double happinessCost = 0.005 * static_cast<double>(IntToFloat(static_cast<int32_t>(6u - static_cast<uint32_t>(mCurrentPayGrade))));
    mHappiness = ClampedMinus(mHappiness, static_cast<float>(happinessCost));
}

// 0x10126e28: working costs 0.025 energy (a single precision product) and 0.01 happiness (a double product) a turn for each grade below 6.
void CStaff::GetTiredWorking() {
    const float energyCost = 0.025f * IntToFloat(static_cast<int32_t>(6u - static_cast<uint32_t>(mCurrentPayGrade)));
    mEnergy = ClampedMinus(mEnergy, energyCost);
    const double happinessCost = 0.01 * static_cast<double>(IntToFloat(static_cast<int32_t>(6u - static_cast<uint32_t>(mCurrentPayGrade))));
    mHappiness = ClampedMinus(mHappiness, static_cast<float>(happinessCost));
}

// 0x10126f30: wcsncpy of 0x20 characters (the rest padded with zeros), then the last character cleared.
void CStaff::SetName(const char16_t* name) {
    int32_t i = 0;
    for (; i < 0x20 && name[i] != 0; ++i) mName[i] = name[i];
    for (; i < 0x20; ++i) mName[i] = 0;
    mName[0x20] = 0;
}

// 0x10126f98
const char16_t* CStaff::GetName() const {
    return mName;
}
// 0x10126fc8: the wage of his type and grade.
uint32_t CStaff::GetSalary() {
    const StaffType type = StaffTypeOfKind(mThing.mKind);
    return static_cast<uint32_t>(gMainBalance.mValues[kMainBalancePayMultiplier + static_cast<int32_t>(type)].mInteger) *
           static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceBaseWage + kMainBalanceStaffGradeStride * mCurrentPayGrade].mInteger);
}

// 0x101270b0: the same for a type and a grade given.
uint32_t CStaff::GetSalary(StaffType type, uint8_t grade) {
    return static_cast<uint32_t>(gMainBalance.mValues[kMainBalancePayMultiplier + static_cast<int32_t>(type)].mInteger) *
           static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceBaseWage + kMainBalanceStaffGradeStride * grade].mInteger);
}

// 0x10127108: the nearest object that chills you out (flag 2) by the squared distance of the cells (the first of equal distances wins; the sum is a
// 32-bit unsigned). The list head is read before the trace (the Mac leaves it in r4 for the trace, which only logs the text).
ThingId CStaff::FindNearestRestingPlace() {
    uint32_t best = 0xffffffffu;
    ThingId bestId{0};
    uint16_t current = gWorld->mF1DA746;
    TRACE(kLookingForRest);
    while (current != kNullThing) {
        const ThingId id{current};
        CObject* object = ObjectOf(RowOf(id));
        if ((object->mFlags & 2) != 0) {
            const MapId objectCell = CellOfThing(*reinterpret_cast<CThing*>(object));
            const MapId myCell = CellOfThing(mThing);
            const uint32_t dx = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(CellX(objectCell) - CellX(myCell)))));
            const uint32_t dy = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(CellY(objectCell) - CellY(myCell)))));
            const uint32_t distance = dx * dx + dy * dy;
            if (distance < best) {
                bestId = id;
                best = distance;
            }
        }
        current = object->mF0C;
    }
    return bestId;
}

// 0x101272b0: the general decisions of a staff member. A striking staff type with the park gates open (status 1) walks to a random spot in the
// strike area (the four random numbers in this order: x cell, x offset, y cell, y offset) and answers 1; with no way there it falls through to the
// energy test. Above the rest level the walking speed follows the energy, and a low (or very high) happiness shows a bubble; below it the staff
// member goes to the nearest rest area (answers 1), or loses happiness one turn in sixteen.
int32_t CStaff::DoGeneralStaffThing() {
    const StaffType type = StaffTypeOfKind(mThing.mKind);
    if (gWorld->GetStaffHQ()->IsStriking(type) != 0 && gWorld->ParkGates_GetStatus() == 1) {
        CTRACE(kTraceLevelStrike, kStrike);
        mF188 = 1;
        MapPos x;
        MapPos y;
        const uint32_t sizeX = static_cast<uint32_t>(BalanceAt(kMainBalanceStrikeAreaSizeX));
        const uint32_t startX = static_cast<uint32_t>(BalanceAt(kMainBalanceStrikeAreaStartX));
        x.mCell = static_cast<uint8_t>(startX + UnsignedRemainder(gWorld->GetRand(), sizeX));
        x.mOffset = static_cast<uint8_t>(gWorld->GetRand());
        const uint32_t sizeY = static_cast<uint32_t>(BalanceAt(kMainBalanceStrikeAreaSizeY));
        const uint32_t startY = static_cast<uint32_t>(BalanceAt(kMainBalanceStrikeAreaStartY));
        y.mCell = static_cast<uint8_t>(startY + UnsignedRemainder(gWorld->GetRand(), sizeY));
        y.mOffset = static_cast<uint8_t>(gWorld->GetRand());
        if (AsPeep(this)->SetDest(x, y) != 0) {
            SetState(static_cast<StaffState>(kStateWalkingToStrike));
            return 1;
        }
        CTRACE(kTraceLevelDefault, kStrikeNoWay);
    }
    const int32_t energy = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(mEnergy)));
    if (energy > BalanceAt(kMainBalanceRestLevel)) {
        mWalkSpeed = static_cast<uint32_t>(energy) < 100u ? kWalkSpeeds[energy / 20] : kWalkSpeeds[4];
        const uint8_t happiness = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(mHappiness)));
        if (happiness <= 10) {
            Thoughts()->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(kThoughtUnhappy), false);
        } else if (happiness > 0x61) {
            if ((gWorld->GetRand() & 0xfu) == 0) Thoughts()->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(kThoughtHappy), false);
        }
        return 0;
    }
    Thoughts()->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(kThoughtTired), false);
    const ThingId room = GoToRestArea();
    if (room.mIndex != kNullThing) {
        SetState(static_cast<StaffState>(kStateWalkingToRest));
        return 1;
    }
    CTRACE(kTraceLevelDefault, kNoRestArea);
    if ((gWorld->GetRand() & 0xfu) == 0) mHappiness = ClampedMinus(mHappiness, IntToFloat(BalanceAt(kMainBalanceHappyHitCosNoRestArea)));
    return 0;
}

// 0x10127658: walks to the entry of the nearest rest area (the object's +0x32); the answer is the room's id when there is a way, else 0 (or the null id
// when there is no room).
ThingId CStaff::GoToRestArea() {
    ThingId room = FindNearestRestingPlace();
    if (room.mIndex != kNullThing) {
        CObject* object = ObjectOf(RowOf(room));
        if (AsPeep(this)->SetDest(MapId{object->mF32}) != 0) {
            mRestArea = room;
            TRACE(kFoundRest);
        } else {
            TRACE(kNoWayToRest);
            room.mIndex = 0;
        }
    }
    return room;
}

// 0x10127764: leaves the rest area: the message centre is told, the room's script variable 0 (the people inside) goes down, the rest area is cleared and
// the staff member's standing script starts.
void CStaff::StopResting() {
    const ThingId id = mThing.GetThingId();
    const CMsgStaffRoomEvent message(id.mIndex, 0);
    PostRoomEvent(message);
    CObject* room = ObjectOf(RowOf(mRestArea));
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(room->mF20));
    RSSE_Setvar(script, 0, RSSE_Getvar(script, 0) - 1);
    mRestArea.mIndex = 0;
    Script()->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(kScriptStanding), mSpriteSet, mSpriteIndex);
    SetState(static_cast<StaffState>(kStateIdle));
}

// 0x101278f0: the patrol area is the box of map cells between the two (the high halfwords of the registers).
void CStaff::SetPatrolArea(MapId bottomLeft, MapId topRight) {
    mPatrolRegionBL = bottomLeft;
    mPatrolRegionTR = topRight;
}

// 0x1012798c
void CStaff::GetPatrolArea(MapId* start, MapId* end) const {
    *start = mPatrolRegionBL;
    *end = mPatrolRegionTR;
}

// 0x10127a24: the cancelled area is the values HasPatrolArea tests for (bottom left 1, top right 0x4000).
void CStaff::CancelPatrolArea() {
    mPatrolRegionBL.mIndex = kPatrolBottomLeftNone;
    mPatrolRegionTR.mIndex = kPatrolTopRightNone;
}

// 0x10127a78
int32_t CStaff::HasPatrolArea() const {
    if (mPatrolRegionBL.mIndex != kPatrolBottomLeftNone) return 1;
    if (mPatrolRegionTR.mIndex != kPatrolTopRightNone) return 1;
    return 0;
}

// 0x10127af8: the cell lies in the box (the compares are signed).
int32_t CStaff::InsidePatrolArea(MapId cell) const {
    const int32_t x = static_cast<int32_t>(CellX(cell));
    const int32_t y = static_cast<int32_t>(CellY(cell));
    if (x >= static_cast<int32_t>(CellX(mPatrolRegionBL)) && x <= static_cast<int32_t>(CellX(mPatrolRegionTR)) &&
        y >= static_cast<int32_t>(CellY(mPatrolRegionBL)) && y <= static_cast<int32_t>(CellY(mPatrolRegionTR))) {
        return 1;
    }
    return 0;
}

// 0x10127c10: up to 30 random cells of the patrol area (the box's width and height are unsigned, an inverted box gives a huge one) until one is on the
// map, is a path and can be walked to; without one the staff member shows the bubble 0x16 and the answer is 0.
int32_t CStaff::FindDestInPatrolArea() {
    const uint32_t width = CellX(mPatrolRegionTR) + 1u - CellX(mPatrolRegionBL);
    const uint32_t height = CellY(mPatrolRegionTR) + 1u - CellY(mPatrolRegionBL);
    for (int32_t tries = 0; tries < kPatrolTries; ++tries) {
        const uint32_t firstX = CellX(mPatrolRegionBL);
        const uint32_t randX = gWorld->GetRand();
        const uint32_t firstY = CellY(mPatrolRegionBL);
        const int32_t x = static_cast<int32_t>(firstX + UnsignedRemainder(randX, width));
        const uint32_t randY = gWorld->GetRand();
        const int32_t y = static_cast<int32_t>(firstY + UnsignedRemainder(randY, height));
        if (CMap::IsXYValid(x, y) == 0) continue;
        const MapId cell{static_cast<uint16_t>(static_cast<uint16_t>(x) + static_cast<uint16_t>(y) * 0x80 + 1)};
        if (MapCellOf(cell)->IsPath() == 0) continue;
        if (AsPeep(this)->SetDest(cell) != 0) return 1;
    }
    TRACE(kNoPatrolDest);
    Thoughts()->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(kThoughtNoPatrolDest), false);
    return 0;
}

// 0x10127dfc
int32_t CMsgStaffRoomEvent::GetType() const { return 15; }
