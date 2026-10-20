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
#include <cstdint>
#include <cstdlib>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr int32_t kKindMechanic = 4;
constexpr int32_t kKindHandyman = 5;
constexpr int32_t kKindEntertainer = 6;
constexpr int32_t kKindGuard = 7;
constexpr int32_t kKindResearcher = 8;
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStatePatrolling = 1;
constexpr int32_t kStateWalkingToRest = 2;
constexpr int32_t kStateResting = 3;
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
constexpr uint32_t kTraceLevelDefault = 2;
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
const char kInvalidType[] = "Invalid staff type\n";                                                          // 0x10245b30
const char kLookingForRest[] = "Looking for rest area...\n";                                                  // 0x10245b52
const char kStrike[] = "Right!  I'm fed up, I'm going on strike!  And I'm going to that bus stop!\n";      // 0x10245b6c
const char kStrikeNoWay[] = "*** Could not get to strike location ***\n";                                   // 0x10245bb7
const char kNoRestArea[] = "Staff member couldn't find a rest area\n";                                       // 0x10245be1
const char kFoundRest[] = "Found a rest area!";                                                              // 0x10245c09
const char kNoWayToRest[] = "Staff: couldn't get to nearest rest area...\n";                                // 0x10245c1c
const char kNoPatrolDest[] = "Could not find or reach a destination in the patrol area\n";                  // 0x10245c49

CPeep* AsPeep(CStaff* s) { return reinterpret_cast<CPeep*>(s); }

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

// The kind byte to the staff type (as src/Staff_queries.cpp's); any other kind asserts and answers 5.
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
