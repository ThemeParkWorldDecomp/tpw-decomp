// CStaff, part 2 (main app, SimTheme Park 2.04b3, staff.cpp; unit w190): SackMe 0x10125660, TrainMe 0x10125854, GetEmploymentDuration 0x10125a10,
// CanBePickedUp 0x10125aec, PickMeUp 0x10125b98, DropMe 0x10125de4, BeingPointedAt 0x10125f30, GetStaffType 0x10126718, GetGeneralStaffState
// 0x101267cc, GetSkill 0x1012686c, IsStriking 0x10126904, IsAllocatedToRide 0x101269d8, IsWorkingOnAllocatedObject 0x10126aac,
// IsRestingInStaffRoom 0x10126c1c, TooTiredToWork 0x10126cb4, SetName 0x10126f30, GetName 0x10126f98 and the two GetSalary (0x10126fc8 and the
// static one 0x101270b0). Part 1 is src/Staff.cpp (unit w189).
//
// The kind byte of the thing (+2) names the staff class: 5 handyman, 4 mechanic, 6 entertainer, 7 guard, 8 researcher. The staff type the
// pay tables and the staff headquarters use is another numbering (0 handyman, 1 mechanic, 2 entertainer, 3 guard, 4 researcher).
#include <cstdint>
#include <cstring>
#include <new>

#include "Debug.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int64_t kTicksPerDay = 864000000000LL;
constexpr int32_t kKindMechanic = 4;
constexpr int32_t kKindHandyman = 5;
constexpr int32_t kKindEntertainer = 6;
constexpr int32_t kKindGuard = 7;
constexpr int32_t kKindResearcher = 8;
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStateResting = 3;
constexpr int32_t kStatePickedUp = 7;
constexpr int32_t kStateCleaningToilet = 0xb;
constexpr int32_t kStateRepairing = 0xd;
constexpr int32_t kStateBeingStunned = 0x12;
constexpr int32_t kMouseModeWalking = 6;              // CMouseMode::GetMode answer of the mode a picked up staff member leaves
constexpr int32_t kHistoryStaffWages = 3;             // as src/Staff.cpp
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

// The Mac makes the mouse and its default mode in this order inline (as src/Staff.cpp and src/Mouse_staff.cpp); the answer is NULL when the mouse
// could not be made. (The same body as src/Staff.cpp's, which keeps its own copy in an anonymous namespace.)
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

// 0x10125aec: not struck (general state 4 or 5), not in state 0xd or 0x12 and inside the park.
int32_t CStaff::CanBePickedUp() {
    if (GetGeneralStaffState() != 4 && GetGeneralStaffState() != 5 && mState != kStateRepairing && mState != kStateBeingStunned &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
        return 1;
    }
    return 0;
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

// 0x10126904: whether his staff type is on strike.
int32_t CStaff::IsStriking() {
    const StaffType type = StaffTypeOfKind(mThing.mKind);
    return gWorld->GetStaffHQ()->IsStriking(type);
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

// 0x1012686c: (grade + percentage / 100) * 20, as an unsigned word (CodeWarrior's double to unsigned conversion). The grade is made a single
// first (fsubs), the percentage a double.
int32_t CStaff::GetSkill() const {
    const float grade = static_cast<float>(mCurrentPayGrade);
    const double sum = static_cast<double>(grade) + static_cast<double>(mPercentageThroughGrade) / 100.0;
    const float scaled = 20.0f * static_cast<float>(sum);
    return static_cast<int32_t>(cw_cvt_fp2unsigned(static_cast<double>(scaled)));
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
