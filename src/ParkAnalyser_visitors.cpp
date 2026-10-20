// The park analyser's visitor counts and opinions (main app, analyser.cpp range; unit w154): 19 functions, 3,896 bytes. CPeep has no layout
// in the headers, so a visitor's fields are read through the Mac offsets named below.
//
// Each walk asserts, once per thing it looks at, that entry 0 of the thing table is free (the text at 0x10240242, the same one
// ParkAnalyser_readers.cpp uses); the release GAME_ASSERTMSG does nothing, so the asserts only show in a call log. A thing is reached through
// gThingArray[id].mThing. The end id of each walk is a 16-bit word in initialised data (all 0, never written): 0x102a0454 (GetHappinessLevel),
// 0x102a045a (GetVisitorsInPark), 0x102a04c8 (NumberOfVisitorsOutsidePark), 0x102a04ca (toilet), 0x102a04cc (hungry), 0x102a04ce (thirsty),
// 0x102a04d0 (balloons), 0x102a04d2 (costumes), 0x102a04d4 (over), 0x102a04d6 (under), 0x102a0534 (GetAverageHappinessAroundPoint).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "History.h"
#include "MapCell.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_main.h"

namespace {

constexpr uint16_t kNoThing = 0;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242

// The kind byte of a person (CPerson); the peeps' fields, Mac offsets (the names are mine).
constexpr uint8_t kKindPerson = 1;
constexpr uint32_t kPeepCostume = 0x24;      // word: 2 = wears a costume
constexpr uint32_t kPeepHappiness = 0x19c;   // float, used as the low byte of its fctiwz
constexpr uint32_t kPeepThirst = 0x1a4;      // float, the same
constexpr uint32_t kPeepHunger = 0x1a8;      // float, the same
constexpr uint32_t kPeepToilet = 0x1ac;      // float, the same
constexpr uint32_t kPeepBalloon = 0x210;     // word: nonzero = holds a balloon
constexpr uint32_t kPeepState = 0x220;       // word: 3 = outside the park (the value NumberOfVisitorsOutsidePark counts)
constexpr int32_t kPeepStateOutside = 3;
constexpr int64_t kTicksPerDay = 864000000000LL;   // 0xc9_2a69c000: 24 hours of 100 ns ticks
constexpr int32_t kCellBytes = 0x44;
constexpr int32_t kMapEdge = 0x7f;

template <class T>
T PeepWord(const CThing* thing, uint32_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof value);
    return value;
}

// A float level as the original uses it: fctiwz, then the low byte of the integer word.
uint32_t LevelByte(const CThing* thing, uint32_t offset) {
    return static_cast<uint32_t>(ppc_fctiwz(PeepWord<float>(thing, offset))) & 0xffu;
}

// The walk of the used things: for each one, GetWorld and the assert, then (kind 1 and CPeep::IsInsidePark nonzero) `visit(thing)`, then
// GetWorld and GetNextUsedThing. The kind is tested first, IsInsidePark next, the field last.
template <class Visit>
void WalkVisitorsInPark(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == kKindPerson && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) visit(thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The cell of a map cell id: the map plus (id - 1) * 0x44 (id 0 gives the 0x44 bytes before the map).
const CMapCell* CellOf(const CMap* map, uint16_t id) {
    return reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(map) +
                                              static_cast<intptr_t>((static_cast<int32_t>(id) - 1) * kCellBytes));
}

// Counts the visitors in the park whose float level at `offset` has a byte above `level` (unsigned).
int32_t CountLevelAbove(const CParkAnalyser* analyser, uint32_t offset, uint8_t level) {
    int32_t count = 0;
    WalkVisitorsInPark(analyser, [&](const CThing* thing) {
        if (LevelByte(thing, offset) > level) count++;
    });
    return count;
}

// The percentage of the visitors in the park whose happiness byte (signed compare with the level) is above (over) or below it.
int32_t PercentageWithHappiness(const CParkAnalyser* analyser, int32_t level, bool over) {
    int32_t total = 0;
    int32_t hits = 0;
    WalkVisitorsInPark(analyser, [&](const CThing* thing) {
        total++;
        const int32_t happiness = static_cast<int32_t>(LevelByte(thing, kPeepHappiness));
        if (over ? happiness > level : happiness < level) hits++;
    });
    return total > 0 ? (hits * 100) / total : 0;
}

}  // namespace

// 0x100e4360: the opinion's history (the elements are 0x40 bytes apart from 0x216fc) gets the value.
void CParkAnalyser::OfferOpinionOn(int32_t opinion, uint8_t value) {
    mOpinionHistory[opinion].SetAtEnd(value);
}

// 0x100e43e4: the three counters are stored in the original's order (mMisbehavingKids, mMisbehavingKidsEver, then, only for `caught` 0, the
// kids that got away with it).
void CParkAnalyser::IveMisbehaved(int32_t caught) {
    mMisbehavingKids++;
    mMisbehavingKidsEver++;
    if (caught == 0) mMisbehavingKidsThatGotAwayWithIt++;
}

// 0x100e4600: the park time of the global world (gGameWorldRef, TOC 0x10260768; not GetWorld) answers the span of that many game turns; its ticks
// are divided by a day (the signed 64-bit helper 0x10237e1c) and the low word is appended to the 21600 history.
void CParkAnalyser::PersonLeftPark(int32_t turns) {
    const TbTimeDiff span = (*gGameWorldRef).mParkTime.GetFunnyTimeForGameTurns(static_cast<uint32_t>(turns));
    mHistory21600.SetAtEnd(static_cast<int32_t>(span.mTicks / kTicksPerDay));
}

// 0x100e4cd8: 0.0f when the gates are shut. Else the mean of the visitors' happiness bytes: the running sum is carried through a float add
// (sum as a float, plus the byte as a float, truncated by fctiwz), so above 2^24 it loses low bits; the answer is float(sum) / float(count).
// The argument is not read.
float CParkAnalyser::GetHappinessLevel(uint32_t) const {
    if (!GatesOpen()) return 0.0f;
    int32_t sum = 0;
    int32_t count = 0;
    WalkVisitorsInPark(this, [&](const CThing* thing) {
        const uint32_t happiness = LevelByte(thing, kPeepHappiness);
        count++;
        sum = ppc_fctiwz(static_cast<float>(sum) + static_cast<float>(happiness));
    });
    if (count < 1) return 0.0f;
    return static_cast<float>(sum) / static_cast<float>(count);
}

// ui_main.h's free shim (HappyBarMsg asks with 4): the member ignores the argument.
float CParkAnalyser_GetHappinessLevel(CParkAnalyser* analyser, int32_t which) {
    return analyser->GetHappinessLevel(static_cast<uint32_t>(which));
}

// 0x100e5414: the number of visitors in the park, cached in mVal1F0B8 for `seconds` seconds: the walk is skipped when the Val has a value (any
// nonzero flag byte) and the seconds since its update (unsigned) are not above `seconds`. The answer is what Get answers.
int32_t CParkAnalyser::GetVisitorsInPark(uint32_t seconds) const {
    // the flag is a byte the original tests against 0 (any nonzero value counts), so it is read as a byte, not as a bool
    uint8_t workedOut;
    std::memcpy(&workedOut, &mVal1F0B8.mWorkedOut, 1);
    if (workedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&mVal1F0B8)->SecondsSinceLastUpdate())) {
        int32_t count = 0;
        WalkVisitorsInPark(this, [&](const CThing*) { count++; });
        const_cast<CParkAnalyserVal<int>&>(mVal1F0B8).Set(count);
    }
    return mVal1F0B8.Get();
}

// 0x100e73a8: for each of the two ticket booths (which 0 and 1) the things of its map cell (kind 1 with the word at +0x220 equal to 3; IsInsidePark is not
// asked) are counted; the count is stored in mVal1F0D0 and the answer is what Get answers (no cache test).
int32_t CParkAnalyser::NumberOfVisitorsOutsidePark() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    int32_t count = 0;
    for (int32_t which = 0; which <= 1; which++) {
        const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> booth = savvy->GetWorld().GetMap()->GetTicketBoothPos(which);
        const uint16_t boothId = booth.mIndex;
        uint16_t cur = CellOf(savvy->GetWorld().GetMap(), boothId)->mWho;
        while (cur != kNoThing) {
            savvy->GetWorld();
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            const CThing* const thing = static_cast<const CThing*>(gThingArray[cur].mThing);
            if (thing->mKind == kKindPerson && PeepWord<int32_t>(thing, kPeepState) == kPeepStateOutside) count++;
            cur = thing->mCellNext;
        }
    }
    const_cast<CParkAnalyserVal<int>&>(mVal1F0D0).Set(count);
    return mVal1F0D0.Get();
}

// 0x100e7520, 0x100e7680, 0x100e77d8: the visitors in the park whose toilet, hunger or thirst byte is above `level` (unsigned bytes).
int32_t CParkAnalyser::NumberOfVisitorsNeedingToilet(uint8_t level) const { return CountLevelAbove(this, kPeepToilet, level); }
int32_t CParkAnalyser::NumberOfHungryVisitors(uint8_t level) const { return CountLevelAbove(this, kPeepHunger, level); }
int32_t CParkAnalyser::NumberOfThirstyVisitors(uint8_t level) const { return CountLevelAbove(this, kPeepThirst, level); }

// 0x100e7930: the kids that got away with it (for any nonzero low byte of the argument) or all the misbehaving kids.
int32_t CParkAnalyser::MisbehavingVisitorsLastMonth(bool gotAway) const {
    return gotAway ? mMisbehavingKidsThatGotAwayWithIt : mMisbehavingKids;
}

// 0x100e7994
int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const { return mLifetimeVisitors; }

// 0x100e79e8: mHistory1F100[10] (+0x20820) is asked for its value `back` places back; the visitors since then, 0 when it has none.
int32_t CParkAnalyser::NumberOfVisitorsInLastMonths(int32_t back) const {
    int32_t value;
    if (mHistory1F100[10].GetValue(value, back)) return mLifetimeVisitors - value;
    return 0;
}

// 0x100e7a94, 0x100e7bd8: the visitors in the park holding a balloon (word +0x210 nonzero) or wearing a costume (word +0x24 equal to 2).
int32_t CParkAnalyser::NumberOfVisitorsWithBalloons() const {
    int32_t count = 0;
    WalkVisitorsInPark(this, [&](const CThing* thing) {
        if (PeepWord<int32_t>(thing, kPeepBalloon) != 0) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfVisitorsWithCostumes() const {
    int32_t count = 0;
    WalkVisitorsInPark(this, [&](const CThing* thing) {
        if (PeepWord<int32_t>(thing, kPeepCostume) == 2) count++;
    });
    return count;
}

// 0x100e7d1c, 0x100e7e98: hits * 100 / visitors in the park (0 when none), a hit being a happiness byte above (below) the signed level.
int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessOver(int32_t level) const { return PercentageWithHappiness(this, level, true); }
int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessUnder(int32_t level) const { return PercentageWithHappiness(this, level, false); }

// 0x100e8018: the mean of the last 50 values of the opinion's history.
uint8_t CParkAnalyser::GetAverageOpinionOn(int32_t opinion) const {
    uint8_t average;
    mOpinionHistory[opinion].GetAverageOfLast(average, 50);
    return average;
}

// 0x100e80a8: the history holds more than 49 values (the original's branchless (x >> 31) + (x >= 50 unsigned) is the signed compare).
bool CParkAnalyser::HaveCompleteSetOfOpinions(int32_t opinion) const {
    return mOpinionHistory[opinion].HowFarBack() > 49;
}

// 0x100ef00c: the cell is 1 based (x = (id - 1) & 0x7f, y = (id - 1) >> 7, id 0 gives x 0x7f and y 0x1ffffff). The ends are x + radius and y + radius,
// replaced by 0x7f when they are above it as UNSIGNED numbers; the starts x - radius and y - radius are not clamped, and the loops compare signed, x
// outermost and y inner (restarted per x). A cell id is the 16-bit (y << 7) + 1 + x, so a negative start wraps to a high cell. In each cell of the
// map CWorldSavvy::GetMap answers, the things of the list from mWho (mCellNext) that are kind 1 are counted and their happiness bytes summed (no
// IsInsidePark test). The answer is the sum over the count (signed divide), 0 when none.
int32_t CParkAnalyser::GetAverageHappinessAroundPoint(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t radius) const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    const uint32_t index = static_cast<uint32_t>(cell.mIndex) - 1u;
    const uint32_t x = index & 0x7fu;
    const uint32_t y = index >> 7;
    uint32_t endX = x + radius;
    if (endX > static_cast<uint32_t>(kMapEdge)) endX = kMapEdge;
    int32_t startX = static_cast<int32_t>(x - radius);
    int32_t startY = static_cast<int32_t>(y - radius);
    uint32_t endY = y + radius;
    if (endY > static_cast<uint32_t>(kMapEdge)) endY = kMapEdge;
    int32_t count = 0;
    int32_t sum = 0;
    for (int32_t cellX = startX; cellX <= static_cast<int32_t>(endX); cellX++) {
        for (int32_t cellY = startY; cellY <= static_cast<int32_t>(endY); cellY++) {
            const uint16_t cellId = static_cast<uint16_t>((static_cast<uint32_t>(cellY) << 7) + 1u + static_cast<uint32_t>(cellX));
            uint16_t cur = CellOf(savvy->GetMap(), cellId)->mWho;
            while (cur != kNoThing) {
                savvy->GetWorld();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
                const CThing* const thing = static_cast<const CThing*>(gThingArray[cur].mThing);
                if (thing->mKind == kKindPerson) {
                    count++;
                    sum += static_cast<int32_t>(LevelByte(thing, kPeepHappiness));
                }
                cur = thing->mCellNext;
            }
        }
    }
    return count != 0 ? sum / count : 0;
}
