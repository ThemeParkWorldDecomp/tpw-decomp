// analyser.cpp: name proven by __sinit_analyser_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "helpers/analyserHelpers.h"
#include "History.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectControl.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "RegionEffectCell.h"
#include "RideDB.h"
#include "Serialise.h"
#include "tb_rain.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ui_main.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <unordered_map>

// CParkAnalyser::CParkAnalyser, main app 0x100e2a4c (1144 bytes), unit w122b.
// Member construction follows the Mac arrays and direct calls; rings and padding stay unwritten.

namespace {
// Copy the timestamp image as words, second word first (0x100e2d3c and the five following copies).
void CopyStamp(uint32_t (&target)[2], const TbTimeStamp& stamp) {
    uint32_t words[2];
    std::memcpy(words, &stamp, sizeof words);
    target[1] = words[1];
    target[0] = words[0];
}
}

CParkAnalyser::CParkAnalyser()
    : mBase(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1}, static_cast<CThing::ThingModel>(0xd)) {
    mLastStrike[1] = 0;
    mLastStrike[0] = 0;
    mLastPuke[1] = 0;
    mLastPuke[0] = 0;
    mLastPrank[1] = 0;
    mLastPrank[0] = 0;
    mParkLastOpened[1] = 0;
    mParkLastOpened[0] = 0;
    mLastObjectBuilt[1] = 0;
    mLastObjectBuilt[0] = 0;
    mLastStaffHired[1] = 0;
    mLastStaffHired[0] = 0;
    mMapSquaresPerCell = 4;
    mMapSizeX = 0x80;
    mMapSizeY = 0x80;
    mGridSizeX = 0x20;
    mGridSizeY = 0x20;
    mMisbehavingKids = 0;
    mMisbehavingKidsThatGotAwayWithIt = 0;
    mMisbehavingKidsEver = 0;
    mLifetimeVisitors = 0;
    CopyStamp(mLastStrike, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastPuke, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastPrank, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mParkLastOpened, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastObjectBuilt, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastStaffHired, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    mMostPaidForTicket = 0;
    mLargestBusLoad = 0;
    mLongestStay = 0;
    Init();
}

// CParkAnalyser::Serialise 0x100e3004 (main app, analyser.cpp range; unit w162): 4,432 bytes, the park analyser's save-file member.
//
// CThing::Serialise (0x1012e010) of the base runs first, whatever save and version are, and a nonzero answer ends the member with 1. Then, only
// when 0 <= version < 2 (two signed compares), save == 0 loads and any other save value saves; any other version answers 0 with nothing more
// called. Each branch is written out straight, as the listing has it twice, in this order, a nonzero answer of any call ending the member with 1
// at once and the end answering 0:
//   mMapSquaresPerCell, mGridSizeX, mGridSizeY, mMapSizeX, mMapSizeY (int), the 0x400 cells, the 4 object-id histories, the six cached values,
//   the 13 + 1 + 2 + 1 + 1 integer and float histories, the 0x14 opinion histories, four counters, six time stamps, three more counters.
// The member calls get (file, save, version, unused) as received (also `save`, which is 0 in the load branch); the VAR calls get
// (file, &field, "name", 0) and never see `unused`.
//
// THE ORIGINAL'S SLIP: the load branch calls SAVEVAR<int> (0x1000f4b0), not LOADVAR, for mMisbehavingKidsEver (0x21c04; listing 0x100e3f6c).

// Declared before its first use: the definition is further down.
template<> int32_t LOADVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog);

int32_t CParkAnalyser::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (save == 0) {
        if (version >= 2) return 0;
        if (version < 0) return 0;
        if (LOADVAR<int32_t>(file, &mMapSquaresPerCell, "mMapSquaresPerCell", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mGridSizeX, "mGridSizeX", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mGridSizeY, "mGridSizeY", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMapSizeX, "mMapSizeX", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMapSizeY, "mMapSizeY", 0) != 0) return 1;
        for (int32_t i = 0; i < 0x400; ++i) {
            if (mCells[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (mLastOfObjectType[i].SerialiseStruct(file, save, version, unused) != 0) return 1;
        }
        if (mVal1F0B8.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0C4.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0D0.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0DC.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0E8.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0F4.Serialise(file, save, version, unused) != 0) return 1;
        for (int32_t i = 0; i < 13; ++i) {
            if (mHistory1F100[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        if (mHistory20F10.Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21160[0].Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21160[1].Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21600.Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory216D8.Serialise(file, save, version, unused) != 0) return 1;
        for (int32_t i = 0; i < 0x14; ++i) {
            if (mOpinionHistory[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        if (LOADVAR<int32_t>(file, &mMisbehavingKids, "mMisbehavingKids", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMisbehavingKidsThatGotAwayWithIt, "mMisbehavingKidsThatGotAwayWithIt", 0) != 0) return 1;
        // The original's load branch saves this field (SAVEVAR, not LOADVAR).
        if (SAVEVAR<int32_t>(file, &mMisbehavingKidsEver, "mMisbehavingKidsEver", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLifetimeVisitors, "mLifetimeVisitors", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStrike), "mLastStrike", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPuke), "mLastPuke", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPrank), "mLastPrank", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mParkLastOpened), "mParkLastOpened", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastObjectBuilt), "mLastObjectBuilt", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStaffHired), "mLastStaffHired", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMostPaidForTicket, "mMostPaidForTicket", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLargestBusLoad, "mLargestBusLoad", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLongestStay, "mLongestStay", 0) != 0) return 1;
        return 0;
    }
    if (version >= 2) return 0;
    if (version < 0) return 0;
    if (SAVEVAR<int32_t>(file, &mMapSquaresPerCell, "mMapSquaresPerCell", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mGridSizeX, "mGridSizeX", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mGridSizeY, "mGridSizeY", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMapSizeX, "mMapSizeX", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMapSizeY, "mMapSizeY", 0) != 0) return 1;
    for (int32_t i = 0; i < 0x400; ++i) {
        if (mCells[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    for (int32_t i = 0; i < 4; ++i) {
        if (mLastOfObjectType[i].SerialiseStruct(file, save, version, unused) != 0) return 1;
    }
    if (mVal1F0B8.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0C4.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0D0.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0DC.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0E8.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0F4.Serialise(file, save, version, unused) != 0) return 1;
    for (int32_t i = 0; i < 13; ++i) {
        if (mHistory1F100[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (mHistory20F10.Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21160[0].Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21160[1].Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21600.Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory216D8.Serialise(file, save, version, unused) != 0) return 1;
    for (int32_t i = 0; i < 0x14; ++i) {
        if (mOpinionHistory[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (SAVEVAR<int32_t>(file, &mMisbehavingKids, "mMisbehavingKids", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMisbehavingKidsThatGotAwayWithIt, "mMisbehavingKidsThatGotAwayWithIt", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMisbehavingKidsEver, "mMisbehavingKidsEver", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLifetimeVisitors, "mLifetimeVisitors", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStrike), "mLastStrike", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPuke), "mLastPuke", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPrank), "mLastPrank", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mParkLastOpened), "mParkLastOpened", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastObjectBuilt), "mLastObjectBuilt", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStaffHired), "mLastStaffHired", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMostPaidForTicket, "mMostPaidForTicket", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLargestBusLoad, "mLargestBusLoad", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLongestStay, "mLongestStay", 0) != 0) return 1;
    return 0;
}

// CParkAnalyser::UpdateTurn (main app 0x100e4188, 4 bytes: a bare return) and CParkAnalyserCell::Init (0x100e6054, 24 bytes), unit w124.

// The park analyser has nothing to do each turn.
void CParkAnalyser::UpdateTurn() {}

// The park analyser's messages, month end, admissions, thoughts and cell save (main app, analyser.cpp range; unit w161): CParkAnalyser::ReceiveMessage
// 0x100e41bc (360 bytes), PersonPaidAdmission 0x100e4470 (216), UpdateEndOfMonth 0x100e47fc (528), GetThreeMostDominantThoughts 0x100e5dc8 (576),
// FinalDestroy 0x100e6fd0 (36), CParkAnalyserCell::Serialise 0x100e60a0 (1,220) and __sinit_analyser_cpp 0x100f1fb0 (28). The messages themselves are in
// src/ParkMessages.cpp. CPeep has no layout in the headers, so the thought of a visitor is read at its Mac offset.

// __sinit_analyser_cpp 0x100f1fb0 (28 bytes): two words of the file's BSS, set at start-up (0 first, then 0x10000). Their use is not known; the names are
// mine (the addresses are their names).
uint32_t gAnalyserWord1034C19C = 0x10000;   // 0x1034c19c
uint32_t gAnalyserWord1034C1A0 = 0;         // 0x1034c1a0

namespace {

constexpr uint16_t kNoThing = 0;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242
constexpr uint8_t kKindPerson = 1;
constexpr uint32_t kPeepThought = 0x30;   // word: the visitor's current thought, 0 .. 0x16 (name mine)
constexpr int32_t kThoughtCount = 0x17;
constexpr uint8_t kNoThought = 0x17;

constexpr int32_t kMsgEndOfMonth = 0xc;
constexpr int32_t kMsgResearchCompleted = 0x11;
constexpr int32_t kMsgEvent = 0x13;
constexpr int32_t kMsgStaffInfo = 0x17;
constexpr int32_t kEventParkOpened = 3;
constexpr int32_t kSituationStrike = 2;

template <class T>
T PeepWord(const CThing* thing, uint32_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof value);
    return value;
}

template <class V>
int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

}  // namespace

// 0x100e41bc (360 bytes). The message type minus 0xc selects (jump table 0x102a0424): 0xc month end, 0x11 research completed, 0x13 event, 0x17 staff
// info; every other type does nothing.
void CParkAnalyser::ReceiveMessage(const CMsg& message) {
    switch (message.GetType()) {
        case kMsgEndOfMonth:
            UpdateEndOfMonth();
            return;
        case kMsgResearchCompleted: {
            const CMsgResearchCompleted& done = static_cast<const CMsgResearchCompleted&>(message);
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id{done.mObj};
            const RideInfo* ride = gRideDatabase.GetRideInfo(id);
            SetLastOfObjectTypeResearched(static_cast<UIObjType>(ride->mUIType), id);
            return;
        }
        case kMsgEvent: {
            const CMsgEvent& event = static_cast<const CMsgEvent&>(message);
            if (event.mEvent == kEventParkOpened) {
                CopyStamp(mParkLastOpened, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            }
            return;
        }
        case kMsgStaffInfo: {
            const CMsgStaffInfo& info = static_cast<const CMsgStaffInfo&>(message);
            if (info.mSituation == kSituationStrike) {
                CopyStamp(mLastStrike, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            }
            return;
        }
        default:
            return;
    }
}

// The park analyser's visitor counts and opinions (main app, analyser.cpp range; unit w154): 19 functions, 3,896 bytes. CPeep has no layout
// in the headers, so a visitor's fields are read through the Mac offsets named below.
//
// Each walk asserts, once per thing it looks at, that entry 0 of the thing table is free (the text at 0x10240242, the same one
// ParkAnalyser_readers.cpp uses); the release GAME_ASSERTMSG does nothing, so the asserts only show in a call log. A thing is reached through
// gThingArray[id].mThing. The end id of each walk is a 16-bit word in initialised data (all 0, never written): 0x102a0454 (GetHappinessLevel),
// 0x102a045a (GetVisitorsInPark), 0x102a04c8 (NumberOfVisitorsOutsidePark), 0x102a04ca (toilet), 0x102a04cc (hungry), 0x102a04ce (thirsty),
// 0x102a04d0 (balloons), 0x102a04d2 (costumes), 0x102a04d4 (over), 0x102a04d6 (under), 0x102a0534 (GetAverageHappinessAroundPoint).

namespace {

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

// 0x100e4470 (216 bytes): counts the visitor, keeps the largest price, and posts a CMsgAddToChallenge (type 0xb, value 1, both ids 0) to the message
// centre, which is made on the first post (a failed new leaves the pointer NULL). The answer of PostMessage is ignored.
void CParkAnalyser::PersonPaidAdmission(int32_t price) {
    ++mLifetimeVisitors;
    if (price > mMostPaidForTicket) mMostPaidForTicket = price;
    CMsgAddToChallenge message(static_cast<ChallengeType>(0xb), 1, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                               tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0});
    MessageCentreForPost()->PostMessage(message);
}

// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgAddToChallenge::~CMsgAddToChallenge() {}

// 0x100e4600: the park time of the global world (gGameWorldRef, TOC 0x10260768; not GetWorld) answers the span of that many game turns; its ticks
// are divided by a day (the signed 64-bit helper 0x10237e1c) and the low word is appended to the 21600 history.
void CParkAnalyser::PersonLeftPark(int32_t turns) {
    const TbTimeDiff span = (*gGameWorldRef).mParkTime.GetFunnyTimeForGameTurns(static_cast<uint32_t>(turns));
    mHistory21600.SetAtEnd(static_cast<int32_t>(span.mTicks / kTicksPerDay));
}

// The things' second batch, the small functions (unit w130): CParkAnalyser::BuiltSomething 0x100e46a4 (120 bytes) and NumberOfStaffType 0x100e8144
// (404 bytes), CBankAccount::MakeWithdrawal 0x100f33b0 (172 bytes), CAdvisor::IsPlaying 0x1000c6e0 (84 bytes) and FindHighestScoringSlotInQueue
// 0x1000b900 (216 bytes), CWeather::SetPrecipitationType 0x10139388 (256 bytes).

namespace {
constexpr int32_t kFirstStaffKind = 4;
constexpr int32_t kLastStaffKind = 8;
}  // namespace

// The park's funny time (the Mac's stack result is copied at once) is the time the last object was built.
void CParkAnalyser::BuiltSomething() {
    TbTimeStamp stamp = gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    static_assert(sizeof(stamp) == sizeof(mLastObjectBuilt), "a time stamp is two words");
    std::memcpy(mLastObjectBuilt, &stamp, sizeof(mLastObjectBuilt));
}

// The park analyser's staff queries (unit w155): HiredSomeone 0x100e4750, GetStaffHappinessLevel 0x100e4ebc, GetDaysSinceLastObjectBuilt 0x100e7098,
// GetDaysSinceLastStaffHired 0x100e7188, NumberOfStaff 0x100e8324, NumberOfStaffTooTiredToWork 0x100e84d4, NumberOfStaffTypeWithPatrolArea 0x100e86a4,
// AverageQualityOfStaffType 0x100e88a0, AverageHappinessOfStaffType 0x100e89fc, AverageHappinessOfStaff 0x100e8b64, CurrentNumberOfStaffTypeForHire
// 0x100e8d4c, CurrentResearcherAbility 0x100e8dd8 and MaxResearcherAbility 0x100e8ed8.

namespace {
constexpr uint32_t kStaffGradeOffset = 0x1e4;        // int32_t: the staff's grade (names mine)
constexpr uint32_t kStaffHappinessOffset = 0x1f4;    // float: the happiness, read as fctiwz(f) & 0xff
constexpr uint32_t kStaffNextResearcherOffset = 0x20c;   // uint16_t: the id of the next researcher of the world's list
template <class T>
T StaffField(const CThing* thing, uint32_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(thing) + offset, sizeof(T));
    return value;
}

bool IsStaffKind(int32_t kind) { return kind >= kFirstStaffKind && kind <= kLastStaffKind; }

// The happiness byte: fctiwz of the float, the low byte of the word.
uint32_t HappinessByte(const CThing* thing) {
    return static_cast<uint32_t>(ppc_fctiwz(StaffField<float>(thing, kStaffHappinessOffset))) & 0xff;
}

// The used list's walk (NumberOfStaffType's): per thing GetWorld, the assertion on row 0, the body, then GetWorld().GetNextUsedThing(thing).
template <class Body>
void WalkThings(const CParkAnalyser* analyser, Body body) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        body(thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// Days from `stamp` to the funny time of the world CWorldSavvy answers (0x10237e1c divides the 64 bit span by a day, the low word is kept).
int32_t DaysSince(const CParkAnalyser* analyser, const uint32_t (&stampWords)[2]) {
    static_assert(sizeof(TbTimeStamp) == sizeof(stampWords), "a time stamp is two words");
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    TbTimeStamp now = savvy->GetWorld().mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // The original passes the member's own address (the stamp is read in place).
    TbTimeDiff span = now - *reinterpret_cast<const TbTimeStamp*>(stampWords);
    return static_cast<int32_t>(span.mTicks / kTicksPerDay);
}
}  // namespace

// The park's funny time of the GLOBAL world (gWorld, not GetWorld) is the time the last staff member was hired.
void CParkAnalyser::HiredSomeone() {
    TbTimeStamp stamp = gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    static_assert(sizeof(stamp) == sizeof(mLastStaffHired), "a time stamp is two words");
    std::memcpy(mLastStaffHired, &stamp, sizeof(mLastStaffHired));
}

// 0x100e47fc (528 bytes): the month-end entries, in the original's order.
void CParkAnalyser::UpdateEndOfMonth() {
    mHistory1F100[0].SetAtEnd((*gGameWorldRef).GetBankAccount()->GetBalance());
    static const int kFromTemp[] = {2, 3, 4, 5, 6, 7, 8, 12};
    for (int k : kFromTemp) {
        mHistory1F100[k].SetFromTemp();
        mHistory1F100[k].mTemp = 0;
    }
    mHistory1F100[9].SetAtEnd(GetVisitorsInPark(0));
    mHistory1F100[10].SetAtEnd(NumberOfVisitorsSinceBeginning());
    mHistory1F100[11].SetAtEnd(GetRidesInPark(0));
    mHistory20F10.SetAtEnd(GetHappinessLevel(0));
    mHistory1F100[1].SetAtEnd(GetParkValue(0));
    mMisbehavingKids = 0;
    mMisbehavingKidsThatGotAwayWithIt = 0;
    int32_t mean;
    mHistory21600.GetAverageOfLast(mean, 50);
    mHistory21160[0].SetAtEnd(mean);
    mHistory21160[1].SetAtEnd(GetOverallParkRating());
}

// CParkAnalyser::Init, main app 0x100e4a44 (456 bytes; analyser.cpp), unit w123b.

namespace {
// The lazy centre block at 0x100e4a64 and twice more (repeated before EACH subscription, as in CChallengeHQ::CChallengeHQ): when the global is NULL a
// centre is allocated (operator new answering NULL is tolerated) and constructed, and the result, NULL included, is stored back. The caller reads the
// global again, so a Subscribe that cleared it makes the next block allocate again; Subscribe is called on whatever the global holds.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The 32 bit wrapping arithmetic of the original (mullw, add, subf).
int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
}  // namespace

// Subscribes to the messages 0xc, 0x11 and 0x13 (answers ignored), then gives each of the grid's cells its place: the loop limit is the low 32 bits of
// the signed product of the grid's two sizes, taken once; the grid width, the squares per cell and the map sizes are read again in every iteration.
// col = i - (i / gridX) * gridX and row = i / gridX (signed divides). A cell starts at squares * col and squares * row and is squares wide and high,
// except that a cell reaching past the map (signed squares * (col + 1) > mapSizeX, and likewise for rows, strict) is cut to (mapSize + squares) -
// squares * (col + 1). CParkAnalyserCell::Init (a stand-in) stores the four numbers; its answer is ignored. Answers 1 on every path, including a
// limit that is zero or negative. The cells are addressed as this + 0x20 + i * 0x7c without a bounds test (a grid larger than 1024 cells would run
// past the array, as the original does).
int32_t CParkAnalyser::Init() {
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xc));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x11));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x13));
    const int32_t limit = Mul(mGridSizeX, mGridSizeY);
    for (int32_t i = 0; i < limit; ++i) {
        const int32_t gridX = mGridSizeX;
        const int32_t squares = mMapSquaresPerCell;
        const int32_t row = i / gridX;
        const int32_t col = Sub(i, Mul(row, gridX));
        const int32_t right = Mul(squares, Add(col, 1));
        int32_t width = squares;
        if (right > mMapSizeX) width = Sub(Add(mMapSizeX, squares), right);
        const int32_t bottom = Mul(squares, Add(row, 1));
        int32_t height = squares;
        if (bottom > mMapSizeY) height = Sub(Add(mMapSizeY, squares), bottom);
        auto* cell = reinterpret_cast<CParkAnalyserCell*>(reinterpret_cast<unsigned char*>(this) + 0x20 + static_cast<std::ptrdiff_t>(i) * 0x7c);
        cell->Init(Mul(squares, col), Mul(squares, row), width, height);
    }
    return 1;
}

// The park analyser's cell and map queries (main app, analyser.cpp range; unit w156): CParkAnalyser::GetAnalyserCell 0x100e4c38 (64 bytes),
// GetNumWalkableCells 0x100e571c, GetNumLitteredCells 0x100e57d4 and GetNumDevelopedCells 0x100e588c (124 bytes each), IsNearbyThingOfType 0x100e6c90
// (44 bytes), GetNearbyLitteredCell 0x100e6d38 (516 bytes), GetPercentageLitteredCells 0x100ed738 (336 bytes), MarkAreaPatrolledBy 0x100ee694 (576 bytes),
// PercentageOfAreaPatrolledBy 0x100ee924 (836 bytes), PercentageOfParkCoveredByCameras 0x100eecd8 (272 bytes), ClearMapData 0x100eee30 (108 bytes),
// NumberOfCellsCoveredByObjects 0x100eeed0 (248 bytes), and CParkAnalyserCell::GetNumWalkableCells 0x100e659c (304 bytes), GetNumLitteredCells
// 0x100e670c (288 bytes) and GetNumDevelopedCells 0x100e686c (448 bytes).
//
// A map cell id is 1 based: x = (id - 1) & 0x7f and y = (id - 1) >> 7 (a 32-bit logical shift, so id 0 gives x 0x7f and y 0x1ffffff); the id of (x, y) is the
// halfword x + (y << 7) + 1 and the cell is the 0x44 byte entry (id - 1) of the map (id 0 reads the entry before it). The functions read different maps:
// the three counters ask the CELL as a CWorldSavvy for GetMap once per cell, GetNearbyLitteredCell reads gWorldMap, GetPercentageLitteredCells the map of
// GetWorld(), and the rest the map of gWorld (at +0x2d8). The full-map loops run x from 0x7f down to 0 (outermost) and y from 0x7f down to 0.
// The patrol mark is CMapCell::mF3C (0 for none, 1 for patrolled).

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uint16_t kNoCollector = 0;    // the halfword at 0x102a04c4, compared with a cell's litter collector (0, never written; not gLitterNoCollector)
constexpr uint32_t kNearbyCellsAt = 0x1f020;   // CParkAnalyser::mNearbyCells, addressed from `this` (a wide search writes past its 0x24 entries)
const char kRadiusText[] = "Litter search radius too large.  Reduce search radius or increase size of buffer in CParkAnalyser\n";   // 0x102402a3

// The id of (x, y): the halfword x + (y << 7) + 1 (the arithmetic is the 32-bit word's, cut to 16 bits).
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// The cell of `id` in a map: entry (id - 1), 0x44 bytes each (id 0 is the entry before the map).
CMapCell* CellOf_2(const void* map, uint16_t id) {
    const int32_t offset = (static_cast<int32_t>(id) - 1) * 0x44;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(map) + static_cast<intptr_t>(offset));
}

// The sum of the 0x400 analyser cells' counts, from the last cell down.
template <typename Count>
int32_t SumOverCells(const CParkAnalyser* analyser, Count count) {
    uint32_t sum = 0;
    for (int32_t i = 0x3ff; i >= 0; i--) sum += static_cast<uint32_t>(count(analyser->mCells[i]));
    return static_cast<int32_t>(sum);
}

// A Val-cached count over the cell's rectangle of map cells: recounted when the Val has no value or maxAge is below the seconds since its stamp
// (unsigned), the cells' map asked of the analyser cell (as a CWorldSavvy) once per cell; the answer is always the Val's.
template <typename Counted>
int32_t CellCount(const CParkAnalyserCell* cell, const CParkAnalyserVal<int>& val, uint32_t maxAge, Counted counted) {
    if (val.mWorkedOut == 0 || maxAge < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(cell);
        uint32_t total = 0;
        for (int32_t x = cell->mStartX; x < static_cast<int32_t>(static_cast<uint32_t>(cell->mStartX) + static_cast<uint32_t>(cell->mSizeX)); x++) {
            for (int32_t y = cell->mStartY; y < static_cast<int32_t>(static_cast<uint32_t>(cell->mStartY) + static_cast<uint32_t>(cell->mSizeY)); y++) {
                const uint16_t id = CellId(x, y);
                const CMapCell* mapCell = CellOf_2(savvy->GetMap(), id);
                if (counted(mapCell)) total++;
            }
        }
        const_cast<CParkAnalyserVal<int>&>(val).Set(static_cast<int32_t>(total));
    }
    return val.Get();
}

// percent = hits * 100 / count (signed divide, the product wrapping through 32 bits), 0 when nothing was counted.
int32_t Percent(int32_t hits, int32_t count) {
    if (count == 0) return 0;
    return static_cast<int32_t>(static_cast<uint32_t>(hits) * 100u) / count;
}

// The full-map loop of gWorld's map shared by the percentage and count functions: x from 0x7f down outermost, y from 0x7f down.
template <typename Visit>
void ForEachCellDown(const void* map, Visit visit) {
    for (int32_t x = 0x7f; x >= 0; x--) {
        for (int32_t y = 0x7f; y >= 0; y--) visit(CellId(x, y), CellOf_2(map, CellId(x, y)));
    }
}

// PercentageOfAreaPatrolledBy's four loops: the cells for which `inArea` answers true are counted, the ones with a positive patrol mark are hits.
template <typename InArea>
int32_t PatrolledPercent(const void* map, InArea inArea) {
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        if (!inArea(cell)) return;
        count++;
        if (static_cast<int32_t>(cell->mF3C) > 0) hits++;
    });
    return Percent(hits, count);
}

}  // namespace

// ---- the analyser

// The analyser cell that covers the map cell: mCells[x / squares + gridX * (y / squares)], two signed divides.
CParkAnalyserCell* CParkAnalyser::GetAnalyserCell(MapId id) const {
    const uint32_t index = static_cast<uint32_t>(id.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(index & 0x7fu);
    const int32_t y = static_cast<int32_t>(index >> 7);
    const int32_t row = y / mMapSquaresPerCell;
    const int32_t column = x / mMapSquaresPerCell;
    const int32_t cell = static_cast<int32_t>(static_cast<uint32_t>(column) + static_cast<uint32_t>(mGridSizeX) * static_cast<uint32_t>(row));
    const int32_t offset = static_cast<int32_t>(static_cast<uint32_t>(cell) * 0x7cu + 0x20u);
    return reinterpret_cast<CParkAnalyserCell*>(reinterpret_cast<uintptr_t>(this) + static_cast<intptr_t>(offset));
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

// The argument is not used. The sum of the happiness bytes is carried through single precision: each step converts the sum to a float, adds
// the byte as a float and truncates back (so a sum above 2^24 loses its low bits). The answer is sum / count as floats, 0.0f when no staff.
float CParkAnalyser::GetStaffHappinessLevel(uint32_t) const {
    int32_t sum = 0;
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) {
            const uint32_t byte = HappinessByte(thing);
            count++;
            sum = ppc_fctiwz(static_cast<float>(static_cast<float>(sum) + static_cast<float>(byte)));
        }
    });
    if (count > 0) return static_cast<float>(sum) / static_cast<float>(count);
    return 0.0f;
}

// ui_main.h's free shim (HappyBarMsg asks with 4): the member ignores the argument.
float CParkAnalyser_GetHappinessLevel(CParkAnalyser* analyser, int32_t which) {
    return analyser->GetHappinessLevel(static_cast<uint32_t>(which));
}

// Declared before its first use: the definition is further down.
template <>
int32_t CParkAnalyserVal<int32_t>::Get() const;

// Declared before its first use: the definition is further down.
template <>
void CParkAnalyserVal<int32_t>::Set(int32_t value);

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

// The park analyser's ride superlatives (main app, analyser.cpp range; unit w153): CParkAnalyser::GetMostExcitingGokart 0x100ec484 (376 bytes),
// GetGokartWithMostCrossRoads 0x100ec668 (384), GetLongestTrackRide 0x100ec858 (424) and its int overload 0x100eca98 (384), GetCoasterWithMostLoops
// 0x100ecc80 (376), GetHighestCoaster 0x100ece94 (408), GetAverageToiletQueueLenght 0x100ed0c0 (316), FindRideWithUnconnectedQueue 0x100ed240
// (284), GetAverageExcitementLevelOfAllRides 0x100ed3a0 (400), GetScariestRide 0x100ed57c (344) and GetRidesInPark 0x100e558c (344).
//
// Every one walks the used list of things as HowBusyIsPark does (src/ParkAnalyser_readers.cpp): per thing the world is asked, entry 0 of the thing
// table is asserted free (the text at 0x10240242), the thing is read from gThingArray[id].mThing, the thing is tested and the world gives the
// next. The id that ends a walk is a halfword of initialised data (0x102a0516 and the ones after it, one per function), all 0 and never written.
// A ride is an object (kind byte 3) whose ride record (CRideDatabase::GetRideInfo of the halfword at +0x0e) has mUIType 0.

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectId;

constexpr int32_t kWhichTrackTypeGokart = 1;   // the record's WhichTrackType of a go-kart track

// Walks the used list through the analyser as a CWorldSavvy; `body(thing, id)` is called for every thing, in the listings' order.
template <class Body>
void WalkThings_2(const CParkAnalyser* analyser, Body body) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        body(thing, cur);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The thing is an object whose ride record says it is a ride (mUIType 0).
bool IsRide(CThing* thing) {
    if (thing->mKind != 3) return false;
    CObject* const object = reinterpret_cast<CObject*>(thing);
    return gRideDatabase.GetRideInfo(ObjectId{object->mObjectType})->mUIType == 0;
}

bool IsGokart(CObject* object) {
    return object->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger == kWhichTrackTypeGokart;
}

// The track sections of the ride: all four outs NULL.
int32_t TrackSections(CObject* object) {
    return BMP_CountTrackSections(object->mF24, nullptr, nullptr, nullptr, nullptr);
}

}  // namespace

// The number of rides, cached in mVal1F0C4 like the average above.
int32_t CParkAnalyser::GetRidesInPark(uint32_t seconds) const {
    CParkAnalyserVal<int>& val = const_cast<CParkAnalyserVal<int>&>(mVal1F0C4);
    if (val.mWorkedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        int32_t count = 0;
        WalkThings_2(this, [&](CThing* thing, uint16_t) {
            if (IsRide(thing)) count++;
        });
        val.Set(count);
    }
    return val.Get();
}

int32_t CParkAnalyser::GetNumWalkableCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumWalkableCells(maxAge); });
}

int32_t CParkAnalyser::GetNumLitteredCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumLitteredCells(maxAge); });
}

int32_t CParkAnalyser::GetNumDevelopedCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumDevelopedCells(maxAge); });
}

// The park analyser's readers of the things (main app, analyser.cpp range; unit w125): CParkAnalyser::HowBusyIsPark 0x100e5944 (420 bytes),
// NumberOfVisitorsInPark 0x100e7278 (240 bytes) and CountNeighboursOfType 0x100e6a6c (424 bytes).
//
// Each walk asserts, once per thing it looks at, that entry 0 of the thing table is free (the text at 0x10240242, the same one World_turn.cpp
// uses); the release GAME_ASSERTMSG does nothing, so the asserts only show in a call log. A thing is reached through gThingArray[id].mThing (a NULL
// one would crash the Mac as it does here). The end of a list is the id read from a 16-bit word in initialised data (0x102a045a for HowBusyIsPark,
// 0x102a04c6 for NumberOfVisitorsInPark, 0x102a04c2 for CountNeighboursOfType), all 0 and never written.

namespace {

// How many kind 1 things (people) of the used list are inside the park, asked of the world through the analyser as a CWorldSavvy (the walk
// of HowBusyIsPark and of NumberOfVisitorsInPark is the same, so one body serves both; the call sequence is the listings').
int32_t CountPeopleInsidePark(const CParkAnalyser* analyser) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    int32_t count = 0;
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == 1 && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) count++;
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    return count;
}

}  // namespace

// The visitors: half the visitors in the park (GetVisitorsInPark(0) / 2, rounded toward 0), at most 100, at least 0. The cached count (mVal1F0B8,
// the number of people inside the park, refreshed when the Val has no value or CTimeSig::SecondsSinceLastUpdate answers other than 0) only decides
// whether the answer is 0: its half (rounded toward 0) is read with Get, and when that is negative the answer is 0 whatever the visitors are.
// Otherwise GetVisitorsInPark(0) / 2 is taken (up to three calls: for the 100 test, the 0 test and the answer).
int32_t CParkAnalyser::HowBusyIsPark() const {
    if (mVal1F0B8.mWorkedOut == 0 || reinterpret_cast<const CTimeSig*>(&mVal1F0B8)->SecondsSinceLastUpdate() != 0) {
        const int32_t count = CountPeopleInsidePark(this);
        const_cast<CParkAnalyserVal<int>&>(mVal1F0B8).Set(count);
    }
    int32_t half = mVal1F0B8.Get() / 2;
    if (half >= 0) half = GetVisitorsInPark(0) / 2;
    else half = 0;
    if (half >= 100) return 100;
    half = GetVisitorsInPark(0) / 2;
    if (half < 0) return 0;
    return GetVisitorsInPark(0) / 2;
}

// 0x100e5dc8 (576 bytes): counts the thought of every visitor in the park, then picks the three most common (a thought counts only when strictly
// greater than the one it replaces; ties keep the earlier). Thought 0 is never picked; counts[1] and counts[2] are not cleared before the ranking.
void CParkAnalyser::GetThreeMostDominantThoughts(uint8_t& first, uint8_t& second, uint8_t& third) const {
    int32_t counts[kThoughtCount];
    for (int32_t i = 0; i < kThoughtCount; ++i) counts[i] = 0;
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == kKindPerson && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) {
            ++counts[PeepWord<int32_t>(thing, kPeepThought)];
        }
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    counts[0] = 0;
    uint8_t best[3] = {0, 1, 2};
    for (int32_t i = 3; i < kThoughtCount; ++i) {
        int32_t gain = 0;
        int32_t slot = -1;
        for (int32_t j = 0; j < 3; ++j) {
            const int32_t d = counts[i] - counts[best[j]];
            if (d > gain) {
                gain = d;
                slot = j;
            }
        }
        if (slot >= 0) best[slot] = static_cast<uint8_t>(i);
    }
    first = counts[best[0]] == 0 ? kNoThought : best[0];
    second = counts[best[1]] == 0 ? kNoThought : best[1];
    third = counts[best[2]] == 0 ? kNoThought : best[2];
}

// Four stores and the answer 1; nothing else of the cell is written.
int32_t CParkAnalyserCell::Init(int32_t startX, int32_t startY, int32_t sizeX, int32_t sizeY) {
    mStartX = startX;
    mStartY = startY;
    mSizeX = sizeX;
    mSizeY = sizeY;
    return 1;
}

// 0x100e60a0 (1,220 bytes): for 0 <= version < 2 the four rectangle words (LOADVAR / SAVEVAR of int), then the two float Vals and the seven int Vals;
// the first nonzero answer ends it with 1, the end answers 0 (so does any other version, with no call).
int32_t CParkAnalyserCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    if (Var<int32_t>(save, file, &mStartX, "mStartX") != 0) return 1;
    if (Var<int32_t>(save, file, &mStartY, "mStartY") != 0) return 1;
    if (Var<int32_t>(save, file, &mSizeX, "mSizeX") != 0) return 1;
    if (Var<int32_t>(save, file, &mSizeY, "mSizeY") != 0) return 1;
    if (mVal10.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1C.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal28.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal34.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal40.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal4C.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal58.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal64.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal70.Serialise(file, save, version, unused) != 0) return 1;
    return 0;
}

// ---- the three counters of one analyser cell (0x100e659c, 0x100e670c, 0x100e686c)

// The map cells of the rectangle that are a path or a queue; mVal4C caches the count.
int32_t CParkAnalyserCell::GetNumWalkableCells(uint32_t maxAge) const {
    return CellCount(this, mVal4C, maxAge, [](const CMapCell* cell) { return cell->IsPath() != 0 || cell->IsQueue() != 0; });
}

// The map cells of the rectangle with litter (the litter word is not 0, no call); mVal58 caches the count.
int32_t CParkAnalyserCell::GetNumLitteredCells(uint32_t maxAge) const {
    return CellCount(this, mVal58, maxAge, [](const CMapCell* cell) { return static_cast<int32_t>(cell->mLitter) != 0; });
}

// The map cells of the rectangle that are a path, queue, entrance, exit, bridge or ride (each next predicate only when the last answered 0);
// mVal64 caches the count.
int32_t CParkAnalyserCell::GetNumDevelopedCells(uint32_t maxAge) const {
    return CellCount(this, mVal64, maxAge, [](const CMapCell* cell) {
        return cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsBridge() != 0 ||
               cell->IsRide() != 0;
    });
}

// The cell of `id` (1 based: x = (id - 1) & 0x7f, y = (id - 1) >> 7, the 32-bit difference, so id 0 gives x 0x7f and y 0x1ffffff) and the cells
// within `a` of it, clamped to 0..0x7f (signed compares for the start and for the clamp), are visited x outermost, y inner, both with UNSIGNED
// loop compares (a `y` start above 0x7f gives no cell at all). Each cell's things are followed from the cell's mWho through their mCellNext links; the
// ones whose kind byte equals `model` (compared as a signed int) are counted, or end the walk with 1 when `b` is not 0. The cell is the 0x44 byte
// entry (id - 1) of the map (id 0 reads the entry before it). A cell with a NULL thing in its list would crash, as on the Mac.
int32_t CParkAnalyser::CountNeighboursOfType(CThing::ThingModel model, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id, int32_t a, int32_t b) {
    int32_t count = 0;
    const uint32_t index = static_cast<uint32_t>(id.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(index & 0x7fu);
    const int32_t y = static_cast<int32_t>(index >> 7);
    int32_t startX = x - a;
    if (startX < 0) startX = 0;
    int32_t startY = y - a;
    if (startY < 0) startY = 0;
    int32_t endX = a + x;
    if (endX > 0x7f) endX = 0x7f;
    int32_t endY = a + y;
    if (endY > 0x7f) endY = 0x7f;
    for (uint32_t cellX = static_cast<uint32_t>(startX); cellX <= static_cast<uint32_t>(endX); cellX++) {
        for (uint32_t cellY = static_cast<uint32_t>(startY); cellY <= static_cast<uint32_t>(endY); cellY++) {
            const uint16_t cellId = static_cast<uint16_t>((cellY << 7) + 1u + cellX);
            const CMapCell* cell = reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(gWorldMap) +
                                                                      static_cast<intptr_t>(static_cast<int32_t>((cellId - 1u) * 0x44u)));
            uint16_t cur = cell->mWho;
            while (cur != kNoThing) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
                const CThing* const thing = static_cast<const CThing*>(gThingArray[cur].mThing);
                if (static_cast<int32_t>(model) == static_cast<int32_t>(thing->mKind)) {
                    if (b != 0) return 1;
                    count++;
                }
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
                cur = static_cast<const CThing*>(gThingArray[cur].mThing)->mCellNext;
            }
        }
    }
    return count;
}

int32_t CParkAnalyser::IsNearbyThingOfType(CThing::ThingModel model, MapId id, int32_t a) {
    return CountNeighboursOfType(model, id, a, 1);
}

// The litter search: the cells of gWorldMap in the square of `radius` around `id` (clamped at 0 on the low side by a signed test of the 32-bit difference and
// at 0x7f on the high side; x outermost, y inner) that have litter and whose collector is `who` (or the halfword kNoCollector) are listed from this + 0x1f020;
// the answer is the first listed cell with the smallest |dx| + |dy| (an unsigned compare against 9999 to start with), 0 when none.
MapId CParkAnalyser::GetNearbyLitteredCell(ThingId who, MapId id, int32_t radius) {
    GAME_ASSERTMSG(radius <= 5, kRadiusText);
    const uint32_t index = static_cast<uint32_t>(id.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(index & 0x7fu);
    const int32_t y = static_cast<int32_t>(index >> 7);
    int32_t startX = static_cast<int32_t>(static_cast<uint32_t>(x) - static_cast<uint32_t>(radius));
    if (startX < 0) startX = 0;
    int32_t startY = static_cast<int32_t>(static_cast<uint32_t>(y) - static_cast<uint32_t>(radius));
    if (startY < 0) startY = 0;
    int32_t endX = static_cast<int32_t>(static_cast<uint32_t>(radius) + static_cast<uint32_t>(x));
    if (endX > 0x7f) endX = 0x7f;
    int32_t endY = static_cast<int32_t>(static_cast<uint32_t>(radius) + static_cast<uint32_t>(y));
    if (endY > 0x7f) endY = 0x7f;
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    int32_t found = 0;
    for (int32_t cellX = startX; cellX <= endX; cellX++) {
        for (int32_t cellY = startY; cellY <= endY; cellY++) {
            const uint16_t cellId = CellId(cellX, cellY);
            const CMapCell* cell = CellOf_2(gWorldMap, cellId);
            if (static_cast<int32_t>(cell->mLitter) == 0) continue;
            if (cell->mLitterCollector == who.mIndex || cell->mLitterCollector == kNoCollector) {
                std::memcpy(base + kNearbyCellsAt + 2u * static_cast<uint32_t>(found), &cellId, sizeof(cellId));
                found++;
            }
        }
    }
    uint16_t nearest = 0;
    uint32_t best = 9999;
    for (int32_t i = 0; i < found; i++) {
        uint16_t entry;
        std::memcpy(&entry, base + kNearbyCellsAt + 2u * static_cast<uint32_t>(i), sizeof(entry));
        const int32_t entryY = static_cast<int32_t>((static_cast<uint32_t>(entry) - 1u) >> 7);
        const uint32_t distanceY = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(static_cast<uint32_t>(entryY) - static_cast<uint32_t>(y)))));
        const int32_t entryX = static_cast<int32_t>((static_cast<uint32_t>(entry) - 1u) & 0x7fu);
        const uint32_t distanceX = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(static_cast<uint32_t>(entryX) - static_cast<uint32_t>(x)))));
        const uint32_t distance = distanceX + distanceY;
        if (distance < best) {
            best = distance;
            nearest = entry;
        }
    }
    MapId result;
    result.mIndex = nearest;
    return result;
}

// 0x100e6fd0 (36 bytes): CThing::FinalDestroy of the base.
void CParkAnalyser::FinalDestroy() { mBase.FinalDestroy(); }

// CParkAnalyser::GatesOpen (main app, 0x100e7028, 64 bytes; unit w129).

// The world is the one CWorldSavvy::GetWorld answers (the analyser is passed on and ignored); the gates are open when IsParkClosed (the whole word at
// world +0x1da710) is 0.
bool CParkAnalyser::GatesOpen() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    return world.IsParkClosed() == 0;
}

int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const { return DaysSince(this, mLastObjectBuilt); }

int32_t CParkAnalyser::GetDaysSinceLastStaffHired() const { return DaysSince(this, mLastStaffHired); }

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    return CountPeopleInsidePark(this);
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

// The things of the used list whose kind is one of 4 to 8 (the staff kinds), is `model` itself and is not dead. The walk and its three calls
// of GetWorld (one before the loop, two a thing) are the readers' (src/ParkAnalyser_readers.cpp).
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    int32_t count = 0;
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        const int32_t kind = thing->mKind;
        if (kind >= kFirstStaffKind && kind <= kLastStaffKind && static_cast<int32_t>(model) == kind && thing->mDead == 0) count++;
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    return count;
}

// Dead staff count too (no mDead test, unlike NumberOfStaffType).
int32_t CParkAnalyser::NumberOfStaff() const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfStaffTooTiredToWork() const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind) && reinterpret_cast<const CStaff*>(thing)->TooTiredToWork() != 0) count++;
    });
    return count;
}

// The model is compared as a signed word with the kind byte; 0 means any model.
int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        const int32_t kind = thing->mKind;
        if (IsStaffKind(kind) && (static_cast<int32_t>(model) == kind || static_cast<int32_t>(model) == 0) &&
            reinterpret_cast<const CStaff*>(thing)->HasPatrolArea() != 0) count++;
    });
    return count;
}

// No staff test: any thing whose kind byte equals the model. 32 bit wrap in the products, a signed divide.
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (static_cast<int32_t>(model) == static_cast<int32_t>(thing->mKind)) {
            count++;
            sum += static_cast<uint32_t>(StaffField<int32_t>(thing, kStaffGradeOffset));
        }
    });
    if (count == 0) return 0;
    const int32_t numerator = static_cast<int32_t>(sum * 100u);
    const int32_t denominator = static_cast<int32_t>(static_cast<uint32_t>(count) * 5u);
    return numerator / denominator;
}

int32_t CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel model) const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (static_cast<int32_t>(model) == static_cast<int32_t>(thing->mKind)) {
            count++;
            sum += HappinessByte(thing);
        }
    });
    if (count == 0) return 0;
    return static_cast<int32_t>(sum) / count;
}

int32_t CParkAnalyser::AverageHappinessOfStaff() const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) {
            count++;
            sum += HappinessByte(thing);
        }
    });
    if (count == 0) return 0;
    return static_cast<int32_t>(sum) / count;
}

// The type is a 32 bit enum on the Mac; the database's declaration takes an int32_t.
int32_t CParkAnalyser::CurrentNumberOfStaffTypeForHire(StaffType type) const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    return savvy->GetWorld().mStaffDB.GetNumStaffOfType(static_cast<int32_t>(type));
}

// The world's list of researchers (the head halfword at +0x1da742, the link at +0x20c of each): the balance's ResearchAbility of each one's grade.
int32_t CParkAnalyser::CurrentResearcherAbility() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    uint32_t sum = 0;
    uint16_t cur = savvy->GetWorld().mF1DA742;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        const CThing* const thing = static_cast<const CThing*>(gThingArray[cur].mThing);
        const int32_t grade = StaffField<int32_t>(thing, kStaffGradeOffset);
        const uint16_t next = StaffField<uint16_t>(thing, kStaffNextResearcherOffset);
        const int32_t slot = kMainBalanceResearchAbility + static_cast<int32_t>(static_cast<uint32_t>(grade) * 3u);
        sum += static_cast<uint32_t>(gMainBalance.mValues[slot].mInteger);
        cur = next;
    }
    return static_cast<int32_t>(sum);
}

// Slot 172 times grade 4's research ability (slot 273), 32 bit wrap.
int32_t CParkAnalyser::MaxResearcherAbility() const {
    const uint32_t a = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceMaxResearchersInPark].mInteger);
    const uint32_t b = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceResearchAbility + kMainBalanceResearcherGradeStride * 4].mInteger);
    return static_cast<int32_t>(a * b);
}

// The park analyser's shop and sideshow superlatives (main app, analyser.cpp range; unit w152): 12 functions, 0x100eaa24 to 0x100ec228 (5,352 bytes).
// CParkAnalyser::GetShopWith{Best,Worst}{Value,Satisfaction} (twice: with a CObject::ShopType, and without one for the food and drink types 1, 5 and 6)
// and GetSideshowWith{Best,Worst}{Value,Satisfaction}.
//
// All twelve are one walk: the used-thing walk of HowBusyIsPark (src/ParkAnalyser_readers.cpp), after clearing the caller's result halfword. Per thing
// the test, in the listing's order, each step short-circuiting:
//  1. the kind byte is 3 (an object);
//  2. the filter: the ride record's ShopType slot (+0x164, signed) equals the type (GetDBInfo called once), or is 1, 5 or 6 (up to three separate
//     GetDBInfo calls), or for the sideshows the ride database's record of mObjectType has mUIType (+0x7a8) 2;
//  3. GetObjectAge() > 30 (signed);
//  4. the VALUE variants: the sum of the last 30 of mHistory1A4 as a double, converted to unsigned (__cvt_fp2unsigned); 0 skips the thing; then
//     (converted sum of mHistory22C * 100) / (converted sum of mHistory1A4, summed a third time), unsigned, then compared as a signed word. The listing
//     converts each 1A4 sum twice in a row (the same double, a pure helper): one call here. The SATISFACTION variants: GetCustomerSatisfaction()
//     (a signed word); the sideshow BestSatisfaction first needs the converted 1A4 sum to be nonzero, as the value variants do;
//  5. keep: Best when nothing was kept yet or the new value is strictly greater, Worst when strictly smaller (signed compares).
// The answer is the kept value, 0 when nothing matched. The end id of the walk is a halfword of initialised data (a different one per function), all 0.
//
// QUIRK: GetSideshowWithWorstSatisfaction 0x100ec228 is the same code as GetSideshowWithWorstValue (it answers the value ratio, an original copy-paste).

namespace {

constexpr uint8_t kObjectKind = 3;
constexpr int32_t kMinimumAge = 30;
constexpr int32_t kSideshowUIType = 2;
constexpr uint32_t kHistoryCount = 30;
using ThingIdRef = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>&;

enum class Pick { kBest, kWorst };

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The ride record's ShopType slot, read through GetDBInfo (one call per read).
int32_t ShopTypeOf(const CObject* object) { return object->GetDBInfo()->Value(kRideSlot_ShopType).mInteger; }

// The filter of the shops of one type.
struct OfType {
    int32_t type;
    bool operator()(const CObject* object) const { return type == ShopTypeOf(object); }
};

// The filter of the food and drink shops: the slot is 1, 5 or 6, each compare reads it again through its own GetDBInfo.
struct AnyShop {
    bool operator()(const CObject* object) const { return ShopTypeOf(object) == 1 || ShopTypeOf(object) == 5 || ShopTypeOf(object) == 6; }
};

// The filter of the sideshows: the ride database's record of the object type has UI type 2.
struct Sideshow {
    bool operator()(const CObject* object) const {
        return gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType))->mUIType == kSideshowUIType;
    }
};

// The converted sum of the last 30 of a history (GetSumOfLast, then __cvt_fp2unsigned).
uint32_t SumOf(const CHistory<uint32_t, 30>& history) {
    double sum;
    history.GetSumOfLast(sum, kHistoryCount);
    return cw_cvt_fp2unsigned(sum);
}

// The value variants' number: nothing (skip the thing) when the converted 1A4 sum is 0, else a * 100 / b, unsigned, as a signed word.
// The listing converts the first 1A4 sum twice in a row (one pure call here), and sums the 1A4 history a second time for the divisor.
bool ValueOf(const CObject* object, int32_t& value) {
    if (SumOf(object->mHistory1A4) == 0) return false;
    const uint32_t a = SumOf(object->mHistory22C);
    const uint32_t b = SumOf(object->mHistory1A4);
    value = static_cast<int32_t>(b != 0 ? (a * 100u) / b : 0u);   // divwu by 0 is undefined on the Mac; the sums agree, so b is not 0 here
    return true;
}

// The satisfaction variants' number.
bool SatisfactionOf(const CObject* object, int32_t& value) {
    value = object->GetCustomerSatisfaction();
    return true;
}

// The sideshow BestSatisfaction's number: like the satisfaction, after the 1A4 sum has converted to nonzero.
bool SideshowSatisfactionOf(const CObject* object, int32_t& value) {
    if (SumOf(object->mHistory1A4) == 0) return false;
    value = object->GetCustomerSatisfaction();
    return true;
}

// The walk. `id` is cleared first (through the reference); a kept thing writes its id there.
template <class Filter, class Number>
int32_t Query(const CParkAnalyser* analyser, ThingIdRef id, Pick pick, Filter filter, Number number) {
    const CWorldSavvy* savvy = Savvy(analyser);
    bool found = false;
    int32_t kept = 0;
    id.mIndex = 0;
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == kObjectKind) {
            const CObject* object = reinterpret_cast<const CObject*>(thing);
            int32_t value;
            if (filter(object) && object->GetObjectAge() > kMinimumAge && number(object, value)) {
                const bool better = !found || (pick == Pick::kBest ? kept < value : value < kept);
                if (better) {
                    found = true;
                    kept = value;
                    id.mIndex = cur;
                }
            }
        }
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    return found ? kept : 0;
}

}  // namespace

// 0x100eaa24 (476 bytes)
int32_t CParkAnalyser::GetShopWithBestValue(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kBest, OfType{type}, ValueOf);
}

// 0x100eb294 (520 bytes)
int32_t CParkAnalyser::GetShopWithBestValue(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, AnyShop{}, ValueOf);
}

// 0x100eac7c (476 bytes)
int32_t CParkAnalyser::GetShopWithWorstValue(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, OfType{type}, ValueOf);
}

// 0x100eb504 (520 bytes)
int32_t CParkAnalyser::GetShopWithWorstValue(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, AnyShop{}, ValueOf);
}

// 0x100eaed4 (348 bytes)
int32_t CParkAnalyser::GetShopWithBestSatisfaction(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kBest, OfType{type}, SatisfactionOf);
}

// 0x100eb778 (392 bytes)
int32_t CParkAnalyser::GetShopWithBestSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, AnyShop{}, SatisfactionOf);
}

// 0x100eb0b4 (348 bytes)
int32_t CParkAnalyser::GetShopWithWorstSatisfaction(CObjectShopType type, ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, OfType{type}, SatisfactionOf);
}

// 0x100eb970 (392 bytes)
int32_t CParkAnalyser::GetShopWithWorstSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, AnyShop{}, SatisfactionOf);
}

// 0x100ebb68 (488 bytes)
int32_t CParkAnalyser::GetSideshowWithBestValue(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, Sideshow{}, ValueOf);
}

// 0x100ebdbc (488 bytes)
int32_t CParkAnalyser::GetSideshowWithWorstValue(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, Sideshow{}, ValueOf);
}

// 0x100ec014 (416 bytes)
int32_t CParkAnalyser::GetSideshowWithBestSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kBest, Sideshow{}, SideshowSatisfactionOf);
}

// 0x100ec228 (488 bytes): the value ratio, not the satisfaction (an original copy-paste of GetSideshowWithWorstValue).
int32_t CParkAnalyser::GetSideshowWithWorstSatisfaction(ThingIdRef id) const {
    return Query(this, id, Pick::kWorst, Sideshow{}, ValueOf);
}

// The go-kart ride with the highest excitement level; `id` is cleared first and takes the id of each better ride. A tie keeps the first.
uint8_t CParkAnalyser::GetMostExcitingGokart(ThingId& id) const {
    bool found = false;
    uint8_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (!IsGokart(object)) return;
        const uint8_t level = object->GetExcitementLevel(false);
        if (!found || best < level) {
            best = level;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The go-kart ride with the most cross roads: the third out of BMP_CountTrackSections, a signed compare.
int32_t CParkAnalyser::GetGokartWithMostCrossRoads(ThingId& id) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (!IsGokart(object)) return;
        int32_t crossRoads;   // the stack word the third out points at (unset if the callee leaves it, as on the Mac)
        BMP_CountTrackSections(object->mF24, nullptr, nullptr, &crossRoads, nullptr);
        if (!found || best < crossRoads) {
            best = crossRoads;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The track ride (IsTrackRide nonzero) of object type `type` (0, the halfword at 0x102a051c, means any) with the most track sections (signed).
int32_t CParkAnalyser::GetLongestTrackRide(ThingId& id, ObjectId type) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsTrackRide() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        const int32_t count = TrackSections(object);
        if (!found || best < count) {
            best = count;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The ride whose record's WhichTrackType is `track` (signed word compare) with the most track sections.
int32_t CParkAnalyser::GetLongestTrackRide(ThingId& id, int32_t track) const {
    bool found = false;
    int32_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (track != object->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger) return;
        const int32_t count = TrackSections(object);
        if (!found || best < count) {
            best = count;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The coaster (the ride record is not asked) of object type `type` (0 means any) with the most loops, the fourth out of GetCoasterTrackStats (unsigned).
uint32_t CParkAnalyser::GetCoasterWithMostLoops(ThingId& id, ObjectId type) const {
    bool found = false;
    uint32_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsCoaster() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        float a, height, c;
        uint32_t loops;
        GetCoasterTrackStats(object, &a, &height, &c, &loops);
        if (!found || best < loops) {
            best = loops;
            found = true;
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The coaster of object type `type` with the greatest second track statistic (the height). The best so far is a uint32_t; it is compared as a float
// (the exact double of the unsigned word, rounded to single) with the new height, and the new best is the height converted back by CodeWarrior's
// double to unsigned helper. A NaN height never wins a compare (the listing's ble is taken for unordered) but is taken when nothing was found yet.
uint32_t CParkAnalyser::GetHighestCoaster(ThingId& id, ObjectId type) const {
    bool found = false;
    uint32_t best = 0;
    id.mIndex = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if (object->IsCoaster() == 0) return;
        if (type.mIndex != kNoThing && object->mObjectType != type.mIndex) return;
        float a, height, c;
        uint32_t loops;
        GetCoasterTrackStats(object, &a, &height, &c, &loops);
        if (!found || static_cast<float>(best) < height) {
            found = true;
            best = cw_cvt_fp2unsigned(height);
            id.mIndex = cur;
        }
    });
    return found ? best : 0;
}

// The mean queue length of the toilets (objects with flag 1): the sum over the count, both as floats; 0.0f when there are none.
float CParkAnalyser::GetAverageToiletQueueLenght() const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t) {
        if (thing->mKind != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        if ((object->mFlags & 1) == 0) return;
        count++;
        sum += static_cast<uint32_t>(object->GetNumPeopleInQueue());
    });
    if (count == 0) return 0.0f;
    return static_cast<float>(static_cast<int32_t>(sum)) / static_cast<float>(count);
}

// The first object with flags 4 and 8 whose queue is not connected, answered when `any` or when it is not the cell system's current object. This
// walk asks the global world (gWorld) directly, not CWorldSavvy::GetWorld, and asserts before each thing.
ThingId CParkAnalyser::FindRideWithUnconnectedQueue(bool any) {
    uint16_t cur = gWorld->GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == 3) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            if ((object->mFlags & 4) != 0 && (object->mFlags & 8) != 0 && object->IsBOQConnected() == 0) {
                if (any) return ThingId{cur};
                if (LLCM_GetCurrentObject() != object) return ThingId{cur};
            }
        }
        cur = gWorld->GetNextUsedThing(thing).mIndex;
    }
    return ThingId{0};
}

// The mean excitement level of the rides (integer division, signed), cached in mVal1F0F4: the walk is skipped when the Val has a value and
// SecondsSinceLastUpdate is not above `seconds` (unsigned compare).
int32_t CParkAnalyser::GetAverageExcitementLevelOfAllRides(uint32_t seconds) const {
    CParkAnalyserVal<int>& val = const_cast<CParkAnalyserVal<int>&>(mVal1F0F4);
    if (val.mWorkedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        int32_t sum = 0;
        int32_t count = 0;
        WalkThings_2(this, [&](CThing* thing, uint16_t) {
            if (!IsRide(thing)) return;
            CObject* const object = reinterpret_cast<CObject*>(thing);
            count++;
            sum += object->GetExcitementLevel(false);
        });
        if (count > 0) val.Set(sum / count);
        else val.Set(0);
    }
    return val.Get();
}

// The ride with the highest excitement level (unsigned byte): no found flag, the best id (0 at the start, 0 meaning none) is written through `id`
// once, after the walk. The first ride is taken because the best id is still 0.
uint8_t CParkAnalyser::GetScariestRide(ThingId& id) const {
    uint16_t bestId = 0;
    uint8_t best = 0;
    WalkThings_2(this, [&](CThing* thing, uint16_t cur) {
        if (!IsRide(thing)) return;
        CObject* const object = reinterpret_cast<CObject*>(thing);
        const uint8_t level = object->GetExcitementLevel(false);
        if (bestId == kNoThing || best < level) {
            best = level;
            bestId = cur;
        }
    });
    id.mIndex = bestId;
    return best;
}

// Percent of the cells of GetWorld()'s map that are walkable (a path or a queue) or littered which are littered. The accessor is asked once, first.
int32_t CParkAnalyser::GetPercentageLitteredCells() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    const void* const map = savvy->GetWorld().GetMap();
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        const bool walkable = cell->IsPath() != 0 || cell->IsQueue() != 0;
        const bool littered = static_cast<int32_t>(cell->mLitter) != 0;
        if (walkable || littered) count++;
        if (littered) hits++;
    });
    return Percent(hits, count);
}

// The park analyser's rating, value and attraction points (main app, analyser.cpp range; unit w159): CParkAnalyser::GetOverallParkRating 0x100ed8cc
// (3,468 bytes), GetParkValue 0x100e5110 (720 bytes) and GetTotalParkAttractionPoints 0x100e5b1c (616 bytes).
//
// GetOverallParkRating is the sum of 11 terms, each a MIN of a count and a cap. The source's MIN macro evaluated its winning argument twice and
// CodeWarrior inlined the first evaluation (the walk of the used things, a copy of NumberOfVisitorsInPark, NumberOfObjectTypeInPark or
// NumberOfStaffType) and CALLED the real function for the second, which it does only when the walk's value is under the cap. Each walk is the
// readers' (src/ParkAnalyser_readers.cpp): GetWorld().GetUsedThingHead(), then per thing GetWorld(), the assertion that entry 0 of the thing table
// is free, the test, GetWorld().GetNextUsedThing(thing). The end ids are halfwords in initialised data, all 0 and never written (0x102a04c6 for the
// visitors', 0x102a04fa for the objects', 0x102a04d8 for the staff's; GetParkValue's object list 0x102a0458, the attraction points' 0x102a045e).

namespace {

constexpr uint8_t kPersonKind = 1;
// The MIN macro of the source, as the listing has it: `first` is the inlined first evaluation, `second` the real call (made only when the first
// is under the cap), the answer is the cap otherwise.
#define TPW_MIN(first, second, cap) ((first) < (cap) ? (second) : (cap))

// The used-thing walk: `visit(thing)` between the assertion and the call of GetNextUsedThing.
template <class Visit>
void WalkUsedThings(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        visit(thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The object list walk from the world `world` (the head halfword at +0x1da746): `visit(object)`, then the next id (+0x0c). GetWorld is called per
// object only when `savvy` is given (the attraction points read the global world).
template <class Visit>
void WalkObjectList(const CWorldSavvy* savvy, const CWorld& world, Visit visit) {   // savvy null: no GetWorld call per object
    uint16_t cur = world.mF1DA746;
    while (cur != kNoThing) {
        if (savvy != nullptr) savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CObject* const object = static_cast<CObject*>(gThingArray[cur].mThing);
        visit(object);
        cur = object->mF0C;
    }
}

// The inlined copy of NumberOfVisitorsInPark.
int32_t InlineVisitors(const CParkAnalyser* analyser) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        if (thing->mKind == kPersonKind && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) count++;
    });
    return count;
}

// The inlined copy of NumberOfObjectTypeInPark: the objects of that UI type; for rides (type 0) the upgrade byte (+0x4c, read unsigned) must be at
// least `level`.
int32_t InlineObjectTypes(const CParkAnalyser* analyser, int32_t type, int32_t level) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        if (thing->mKind == kObjectKind) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
            if (info->mUIType == type) {
                if (type != 0 || static_cast<int32_t>(object->mF4C) >= level) count++;
            }
        }
    });
    return count;
}

// The inlined copy of NumberOfStaffType: kinds 4 to 8, the kind equal to `model`, not dead.
int32_t InlineStaff(const CParkAnalyser* analyser, CThing::ThingModel model) {
    int32_t count = 0;
    WalkUsedThings(analyser, [&](CThing* thing) {
        const int32_t kind = thing->mKind;
        if (kind >= kFirstStaffKind && kind <= kLastStaffKind && kind == static_cast<int32_t>(model) && thing->mDead == 0) count++;
    });
    return count;
}

}  // namespace

// The park rating: the 11 terms are computed in the listing's order (the walk of each, then the real call when under the cap) and added at the end.
//  1. the visitors (people inside the park) at most 1000, times 20 / 1000 (signed);  2. the rides (UI type 0) times 3 / 2, at most 20;
//  3. type 1 objects times 2, at most 10;  4. type 2 times 2, at most 10;  5. type 3, at most 10;  6. the rides upgraded to level 2 or more, at most 10;
//  7 to 11. the staff of kinds 5, 4, 6, 7 and 8, at most 4 each.
int32_t CParkAnalyser::GetOverallParkRating() const {
    const int32_t visitorCount = InlineVisitors(this);
    const int32_t visitors = TPW_MIN(visitorCount, NumberOfVisitorsInPark(), 1000);
    const int32_t visitorsTerm = visitors * 20 / 1000;
    const int32_t rides = InlineObjectTypes(this, 0, 0);
    const int32_t ridesTerm = TPW_MIN(rides * 3 / 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 0) * 3 / 2, 20);
    const int32_t type1 = InlineObjectTypes(this, 1, 0);
    const int32_t type1Term = TPW_MIN(type1 * 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(1), 0) * 2, 10);
    const int32_t type2 = InlineObjectTypes(this, 2, 0);
    const int32_t type2Term = TPW_MIN(type2 * 2, NumberOfObjectTypeInPark(static_cast<UIObjType>(2), 0) * 2, 10);
    const int32_t type3 = InlineObjectTypes(this, 3, 0);
    const int32_t type3Term = TPW_MIN(type3, NumberOfObjectTypeInPark(static_cast<UIObjType>(3), 0), 10);
    const int32_t upgraded = InlineObjectTypes(this, 0, 2);
    const int32_t upgradedTerm = TPW_MIN(upgraded, NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 2), 10);
    const int32_t staff5 = InlineStaff(this, static_cast<CThing::ThingModel>(5));
    const int32_t staff5Term = TPW_MIN(staff5, NumberOfStaffType(static_cast<CThing::ThingModel>(5)), 4);
    const int32_t staff4 = InlineStaff(this, static_cast<CThing::ThingModel>(4));
    const int32_t staff4Term = TPW_MIN(staff4, NumberOfStaffType(static_cast<CThing::ThingModel>(4)), 4);
    const int32_t staff6 = InlineStaff(this, static_cast<CThing::ThingModel>(6));
    const int32_t staff6Term = TPW_MIN(staff6, NumberOfStaffType(static_cast<CThing::ThingModel>(6)), 4);
    const int32_t staff7 = InlineStaff(this, static_cast<CThing::ThingModel>(7));
    const int32_t staff7Term = TPW_MIN(staff7, NumberOfStaffType(static_cast<CThing::ThingModel>(7)), 4);
    const int32_t staff8 = InlineStaff(this, static_cast<CThing::ThingModel>(8));
    const int32_t staff8Term = TPW_MIN(staff8, NumberOfStaffType(static_cast<CThing::ThingModel>(8)), 4);
    return visitorsTerm + ridesTerm + type1Term + type2Term + type3Term + upgradedTerm + staff5Term + staff4Term + staff6Term + staff7Term + staff8Term;
}

#undef TPW_MIN

// The park value, cached in mVal1F0E8 for `seconds` seconds (the visitors' cache test: the flag byte is any nonzero value, the seconds compare is
// unsigned). The sum: per map cell (x then y from 127 down to 0; the cell is world + 0x2d8 + (place - 1) * 0x44) without flag 0x40: water 1, path 3,
// queue 5, entrance 1, exit 1, bridge 10, grass 2, track 50; then per object of the object list that is not condemned the sum of the upgrade costs
// of levels 0 to its upgrade byte times the low byte of its mF40 (fctiwz) / 100 (signed, the product wraps).
int32_t CParkAnalyser::GetParkValue(uint32_t seconds) const {
    uint8_t workedOut;
    std::memcpy(&workedOut, &mVal1F0E8.mWorkedOut, 1);
    if (workedOut == 0 || seconds < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&mVal1F0E8)->SecondsSinceLastUpdate())) {
        uint32_t total = 0;
        const CWorldSavvy* savvy = Savvy(this);
        CWorld& world = const_cast<CWorld&>(savvy->GetWorld());
        for (int32_t x = 127; x >= 0; --x) {
            for (int32_t y = 127; y >= 0; --y) {
                const uint16_t place = static_cast<uint16_t>(x + (y << 7) + 1);
                CMapCell* const cell = &world.mMapCells[place - 1];
                if (cell->IsFlag(0x40) == 0) {
                    if (cell->IsWater() != 0) total += 1;
                    if (cell->IsPath() != 0) total += 3;
                    if (cell->IsQueue() != 0) total += 5;
                    if (cell->IsEntrance() != 0) total += 1;
                    if (cell->IsExit() != 0) total += 1;
                    if (cell->IsBridge() != 0) total += 10;
                    if (cell->IsGrass() != 0) total += 2;
                    if (cell->IsTrack() != 0) total += 50;
                }
            }
        }
        WalkObjectList(savvy, savvy->GetWorld(), [&](CObject* object) {
            int32_t value = 0;
            if (object->IsCondemned() == 0) {
                uint32_t sum = 0;
                for (int32_t i = 0; i <= static_cast<int32_t>(object->mF4C); ++i) {
                    const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
                    sum += static_cast<uint32_t>(info->Value(static_cast<RideBalanceSlot>(kRideSlot_Upgrades_CostOfUpgrade + 16 * i)).mInteger);
                }
                const uint32_t scale = static_cast<uint32_t>(ppc_fctiwz(object->mF40)) & 0xff;
                value = static_cast<int32_t>(sum * scale) / 100;
            }
            total += static_cast<uint32_t>(value);
        });
        const_cast<CParkAnalyserVal<int>&>(mVal1F0E8).Set(static_cast<int32_t>(total));
    }
    return mVal1F0E8.Get();
}

// The attraction points: over the object list from the GLOBAL world's head (never GetWorld), for each rides or shops object (UI type 0 or 2) that is
// available to choose and has a positive count in the park: (the ride's new bonus for its age slot, 0 outside 0 to 2, plus its attraction value)
// divided by the count. A ride record whose track type is 0 counts all the type in the park and takes the age from the days since the type's first
// build (the low word of the 64-bit quotient); any other takes count 1 and the object's age. The age slot is age / the decay time (signed divw; a
// decay time of 0 is undefined on the Mac).
int32_t CParkAnalyser::GetTotalParkAttractionPoints() {
    int32_t total = 0;
    CWorld* const world = gWorld;
    WalkObjectList(nullptr, *world, [&](CObject* object) {
        const uint16_t type = object->mObjectType;
        const int32_t uiType = gRideDatabase.GetRideInfo(ObjectIdOf(type))->mUIType;
        if (uiType != 0 && uiType != 2) return;
        if (object->IsAvailableToChoose() == 0) return;
        if (NumberOfObjectInPark(ObjectIdOf(type)) <= 0) return;
        int32_t count;
        int32_t age;
        if (gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_WhichTrackType).mInteger == 0) {
            count = NumberOfObjectInPark(ObjectIdOf(type));
            ObjectControl* const control = gWorld->GetControlManager()->GetObjectControl(ObjectIdOf(type), 0);
            const TbTimeDiff span = gWorld->mParkTime.GetFunnyTimeSinceGameTurn(control->mFirstBuiltTurn);
            age = static_cast<int32_t>(span.mTicks / 864000000000LL);
        } else {
            count = 1;
            age = object->GetObjectAge();
        }
        const int32_t decay = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_NewAttractionDecayTime).mInteger;
        const int32_t slot = age / decay;
        int32_t points;
        if (slot < 3 && slot >= 0) {
            points = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(static_cast<RideBalanceSlot>(kRideSlot_Attraction_NewBonus + slot)).mInteger;
        } else {
            points = 0;
        }
        const int32_t value = gRideDatabase.GetRideInfo(ObjectIdOf(type))->Value(kRideSlot_AttractionValue).mInteger;
        // the sum and the total wrap as the Mac's 32-bit add does
        const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(points) + static_cast<uint32_t>(value));
        total = static_cast<int32_t>(static_cast<uint32_t>(total) + static_cast<uint32_t>(sum / count));
    });
    return total;
}

// Marks the patrol areas of the staff of `model`: ClearMapData first, then every used thing (the walk of gWorld, entry 0 of the thing table asserted free
// before each) whose kind is a staff kind (4 to 8) equal to `model` and that has a patrol area marks the cells of the rectangle from the area's start cell to
// its end cell (x outermost; both signed compares; the bounds are those of the cell ids' x and y, so a start right of the end marks nothing).
void CParkAnalyser::MarkAreaPatrolledBy(CThing::ThingModel model) {
    ClearMapData();
    uint16_t cur = gWorld->GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        const uint8_t kind = thing->mKind;
        const bool staff = kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7;
        if (staff && static_cast<int32_t>(model) == static_cast<int32_t>(kind) && reinterpret_cast<CStaff*>(thing)->HasPatrolArea() != 0) {
            const void* const map = gWorld->GetMap();
            MapId start;
            MapId end;
            reinterpret_cast<CStaff*>(thing)->GetPatrolArea(&start, &end);
            const int32_t firstX = static_cast<int32_t>((static_cast<uint32_t>(start.mIndex) - 1u) & 0x7fu);
            for (int32_t x = firstX; x <= static_cast<int32_t>((static_cast<uint32_t>(end.mIndex) - 1u) & 0x7fu); x++) {
                for (int32_t y = static_cast<int32_t>((static_cast<uint32_t>(start.mIndex) - 1u) >> 7);
                     y <= static_cast<int32_t>((static_cast<uint32_t>(end.mIndex) - 1u) >> 7); y++) {
                    CellOf_2(map, CellId(x, y))->mF3C = 1;
                }
            }
        }
        cur = gWorld->GetNextUsedThing(thing).mIndex;
    }
}

// MarkAreaPatrolledBy(model), then the percent of the cells of one kind of area that are marked (a positive mark): the land (not water, freeform or lake),
// the built cells (path, queue, entrance, exit, bridge, ride, track or coaster pylon), the entrances, or the paths and queues; any other area answers 0.
int32_t CParkAnalyser::PercentageOfAreaPatrolledBy(CThing::ThingModel model, AreaType area) {
    MarkAreaPatrolledBy(model);
    const void* const map = gWorld->GetMap();
    switch (static_cast<int32_t>(area)) {
        case kAreaLand:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsWater() == 0 && cell->IsFreeform() == 0 && cell->IsLake() == 0; });
        case kAreaBuilt:
            return PatrolledPercent(map, [](const CMapCell* cell) {
                return cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsBridge() != 0 ||
                       cell->IsRide() != 0 || cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0;
            });
        case kAreaEntrance:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsEntrance() != 0; });
        case kAreaPath:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsPath() != 0 || cell->IsQueue() != 0; });
        default:
            return 0;
    }
}

// Percent of the land cells (not water, freeform or lake) of gWorld's map whose region effect cell has a positive effect [3].
int32_t CParkAnalyser::PercentageOfParkCoveredByCameras() {
    CMap* const map = gWorld->GetMap();
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t id, const CMapCell* cell) {
        if (cell->IsWater() != 0 || cell->IsFreeform() != 0 || cell->IsLake() != 0) return;
        count++;
        MapId effectId;
        effectId.mIndex = id;
        if (map->GetRegionalEffectCell(effectId)->mpEffect[3] > 0) hits++;
    });
    return Percent(hits, count);
}

// Clears the patrol mark of every cell of gWorld's map.
void CParkAnalyser::ClearMapData() {
    const void* const map = gWorld->GetMap();
    ForEachCellDown(map, [](uint16_t, CMapCell* cell) { cell->mF3C = 0; });
}

// The cells of gWorld's map that are an entrance, exit, ride, track or coaster pylon.
int32_t CParkAnalyser::NumberOfCellsCoveredByObjects() {
    const void* const map = gWorld->GetMap();
    int32_t count = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        if (cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsRide() != 0 || cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0) count++;
    });
    return count;
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

int32_t CMsgAddToChallenge::GetType() const { return 0x1a; }

// CHistory<T, N> and CParkAnalyserVal<int> members (main app, unit w157). The analyser keeps its series (visitors, cash, ratings ...) in rings of
// the last N values. Each member is one body here; the original has one copy per instance (and per source file it was used in), listed in
// include/History.h and below at the explicit instantiations. The bodies read mNumEntries from the object, never N, as the listings do.
// Layout (History.h): mTemp, the ring, mCurrentEntry (the newest, -1 while empty), mNumEntries, mWrappedAround.

// ---- CParkAnalyserVal<int> (main 0x100ef3c8 and 0x100ef400)

template <>
int32_t CParkAnalyserVal<int32_t>::Get() const {   // 0x100ef3c8
    return mVal;
}

// Stores the value, marks it worked out, then stamps the first word (a CTimeSig image) with the world's turn.
template <>
void CParkAnalyserVal<int32_t>::Set(int32_t value) {   // 0x100ef400
    mVal = value;
    mWorkedOut = true;
    reinterpret_cast<CTimeSig*>(this)->SetTimeSig();
}

// The park analyser's save-file members for its value and history templates (main app, analyser.cpp range; unit w158): ten functions in
// 5,280 bytes. All of them read or write the same fields in the same order, the load branch calling LOADVAR and the save branch SAVEVAR, and
// only when 0 <= version < 2 (two signed compares); any other version, and a call that answers nonzero, are described below.
//
//   CParkAnalyserVal<T>::Serialise  <int> 0x100ef278, <float> 0x100ef4a8 (276 bytes each): mVal, then mWorkedOut (mTimeSig is not saved).
//   CHistory<T, N>::Serialise: mCurrentEntry, mNumEntries, mWrappedAround, mTemp, then the elements 0 .. mNumEntries - 1 under the text
//     "mData[i]" (the literal, not a formatted index). <unsigned char,50> 0x100ef644 (584 bytes: the element address needs no shift, the
//     other instances 592), <int,5> 0x100efbbc, <int,50> 0x100efe8c, <float,144> 0x100f0414, <int,144> 0x100f0754, <int,30> 0x10110fec,
//     <unsigned int,30> 0x10111560. One body serves them all: the instances differ in the element's size and its load and save calls.
//   CHistory<ObjectId,3>::SerialiseStruct 0x100f0b80 (592 bytes): the same with LOADVARSTRUCT / SAVEVARSTRUCT of the 16-bit tag for mTemp
//     and the elements.
//
// A call that answers nonzero ends the member at once with 1; the end of the member answers 0 (so does a version outside 0 .. 1, with no
// call made). `unused` (the fourth argument) is never read. noLog is 0 in every call.
//
// The element loop reloads mNumEntries from the object before every iteration, as the listings do (lwz 0x1c(r28) after each call), so a loaded
// element that overlaps the count changes the bound. The element address is the object's address plus the data offset plus i * sizeof(T) for every
// i, also from i = N on, where it is the next fields of the object (mCurrentEntry at i = N and mNumEntries at i = N + 1 for the 4-byte rings): the
// walk is byte arithmetic on the object, never mData[i].

namespace {

// The strings of the original (the table at 0x10240129 + 0x1dd ..).
const char kVal[] = "mVal";
const char kWorkedOut[] = "mWorkedOut";
const char kCurrentEntry[] = "mCurrentEntry";
const char kNumEntries[] = "mNumEntries";
const char kWrappedAround[] = "mWrappedAround";
const char kTemp[] = "mTemp";
const char kData[] = "mData[i]";

template <class V>
int32_t Var_2(bool save, void* file, V* value, const char* name) {
    return save ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

template <class V>
int32_t VarStruct(bool save, void* file, V* value, const char* name) {
    return save ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}

// The common walk of the three history forms; Elem is LOADVAR / SAVEVAR for the plain elements and the STRUCT pair for the tag.
template <class H, class T, class Elem>
int32_t SerialiseHistory(H* self, void* file, int32_t saveFlag, int32_t version, Elem elem) {
    if (version < 0 || version >= 2) return 0;
    const bool save = saveFlag != 0;
    if (Var_2<int32_t>(save, file, &self->mCurrentEntry, kCurrentEntry) != 0) return 1;
    if (Var_2<int32_t>(save, file, &self->mNumEntries, kNumEntries) != 0) return 1;
    if (Var_2<bool>(save, file, &self->mWrappedAround, kWrappedAround) != 0) return 1;
    if (elem(save, file, &self->mTemp, kTemp) != 0) return 1;
    for (int32_t i = 0; i < self->mNumEntries; ++i) {
        T* element = reinterpret_cast<T*>(reinterpret_cast<char*>(self) + offsetof(H, mData) + static_cast<size_t>(i) * sizeof(T));
        if (elem(save, file, element, kData) != 0) return 1;
    }
    return 0;
}

}  // namespace

template <class T>
int32_t CParkAnalyserVal<T>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    const bool saving = save != 0;
    if (Var_2<T>(saving, file, &mVal, kVal) != 0) return 1;
    if (Var_2<bool>(saving, file, &mWorkedOut, kWorkedOut) != 0) return 1;
    return 0;
}

// The mean of the last `count` values, truncated toward zero (fctiwz); 0 when nothing was summed. A one-byte type keeps the low byte.
template <class T, int N>
void CHistory<T, N>::GetAverageOfLast(T& out, int32_t count) const {
    double sum;
    const int32_t n = GetSumOfLast(sum, count);
    if (n > 0) out = static_cast<T>(ppc_fctiwz(sum / static_cast<double>(n)));
    else out = 0;
}

// The sum of the last `count` values. The assert fires on every call whose count is above mNumEntries (and the count is then truncated anyway).
// HowFarBack is called twice when it clamps. The element is converted to double (signed, unsigned or zero-extended by its type). Answers the count.
template <class T, int N>
int32_t CHistory<T, N>::GetSumOfLast(double& sum, int32_t count) const {
    GAME_ASSERTMSG(count <= mNumEntries,
                   "CHistory - You asked for the sum of more elements than there is defined history for\n"
                   "The request will be truncated, and you'll get what we can give\n");
    int32_t n = count;
    if (HowFarBack() < count) n = HowFarBack();
    sum = 0.0;
    for (int32_t i = 0; i < n; ++i) {
        T value;
        if (GetValue(value, i)) sum += static_cast<double>(value);
    }
    return n;
}

template <class T, int N>
int32_t CHistory<T, N>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return SerialiseHistory<CHistory<T, N>, T>(this, file, save, version, [](bool s, void* f, T* p, const char* n) { return Var_2<T>(s, f, p, n); });
}

// Pushes mTemp onto the ring. <int,144> inlines SetAtEnd, <int,30> and <unsigned int,30> call it; the behaviour is the same.
template <class T, int N>
void CHistory<T, N>::SetFromTemp() {
    SetAtEnd(mTemp);
}

template <class T, int N>
int32_t CHistory<T, N>::SerialiseStruct(void* file, int32_t save, int32_t version, uint32_t unused) {
    return SerialiseHistory<CHistory<T, N>, T>(this, file, save, version, [](bool s, void* f, T* p, const char* n) { return VarStruct<T>(s, f, p, n); });
}

template int32_t CParkAnalyserVal<int32_t>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CParkAnalyserVal<float>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<uint8_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 5>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<float, 144>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 144>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<uint32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3>::SerialiseStruct(void*, int32_t, int32_t, uint32_t);

// ---- CHistory

// Advances the newest index, wrapping to 0 (and flagging the wrap) at mNumEntries, then stores the value at the index re-read.
template <class T, int N>
void CHistory<T, N>::SetAtEnd(T value) {
    mCurrentEntry += 1;
    if (mCurrentEntry >= mNumEntries) {
        mCurrentEntry = 0;
        mWrappedAround = true;
    }
    mData[mCurrentEntry] = value;
}

// The value `back` places before the newest. A negative `back` passes the first two tests (plain arithmetic, as the listing does).
template <class T, int N>
bool CHistory<T, N>::GetValue(T& out, int32_t back) const {
    if (back >= mNumEntries) return false;
    if (back <= mCurrentEntry) {
        out = mData[mCurrentEntry - back];
        return true;
    }
    if (mWrappedAround) {
        out = mData[mNumEntries + mCurrentEntry - back];
        return true;
    }
    return false;
}

// The park analyser's queries over the objects and the ride database (main app, analyser.cpp range; unit w151): 26 functions from
// CParkAnalyser::NumberOfToiletsInPark 0x100e8f28 to SetLastOfObjectTypeResearched 0x100ea968.
//
// Three walks, all with the call sequence of the listings:
//  * the used-thing walk of HowBusyIsPark (src/ParkAnalyser_readers.cpp): GetWorld().GetUsedThingHead(), then per thing GetWorld(), the assertion
//    that entry 0 of the thing table is free, the thing from gThingArray[id].mThing, the test, GetWorld().GetNextUsedThing(thing). The end id
//    is a halfword in initialised data (a different one per function), all 0 and never written. An object is a thing of kind 3.
//  * the object list walk (NumberOfBrokenDownRidesInPark, NumberOfWornRidesInPark): from the world's halfword at +0x1da746 (the head of the
//    object list), each object's halfword at +0x0c is the next id, 0 ends. No kind test.
//  * the ride database walk: GetFirstRide, GetNextRide, and per ride the object control of its id, from the CONST GetObjectControl (0x100fa1b8)
//    of the control manager at world + 0x1d943c. A ride record's golden ticket slot (46, +0xc4) is read as a signed word.

namespace {

constexpr uint16_t kToiletFlag = 1;
constexpr uint16_t kStaffRoomFlag = 2;
// The used-thing walk: `visit(id, thing)` is called for every thing of the list, between the assertion and the call of GetNextUsedThing.
template <class Visit>
void WalkUsedThings_2(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        visit(cur, thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// The object list walk: `visit(object)` for every object from the world's list head; the next id is read after it.
template <class Visit>
void WalkObjectList_2(const CParkAnalyser* analyser, Visit visit) {
    const CWorldSavvy* savvy = Savvy(analyser);
    uint16_t cur = savvy->GetWorld().mF1DA746;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CObject* const object = static_cast<CObject*>(gThingArray[cur].mThing);
        visit(object);
        cur = object->mF0C;
    }
}

CObject* AsObject(CThing* thing) { return reinterpret_cast<CObject*>(thing); }

// The object control of a ride record: GetWorld() once, then the const GetObjectControl(id, 0) of the manager at world + 0x1d943c.
const ObjectControl* ControlOf(const CParkAnalyser* analyser, const RideInfo* info) {
    const CControlManager* manager =
        reinterpret_cast<const CControlManager*>(reinterpret_cast<uintptr_t>(&Savvy(analyser)->GetWorld()) + 0x1d943c);
    return manager->GetObjectControl(ObjectIdOf(info->mId), 0);
}

// The golden ticket cost slot as a signed word.
int32_t GoldenTicketCost(const RideInfo* info) { return info->Value(kRideSlot_GoldenTicketCost).mInteger; }

// The filth level of a toilet: 100 minus the low byte of the cleanliness float truncated (fctiwz).
int32_t FilthOf(const CObject* object) { return 100 - (ppc_fctiwz(object->mF40) & 0xff); }

// The constant UI type of AllObjectsResearchedAndPresent's skipped types.
constexpr int32_t kUITypeSkipped = 4;

}  // namespace

// Counts the objects with the toilet flag, hands the count to the cached value (mVal1F0DC) and answers what Get answers afterwards (the
// decompiler shows a void: r3 after Get is the answer).
int32_t CParkAnalyser::NumberOfToiletsInPark() const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) count++;
    });
    const_cast<CParkAnalyserVal<int>&>(mVal1F0DC).Set(count);
    return mVal1F0DC.Get();
}

int32_t CParkAnalyser::NumberOfClosedToiletsInPark() const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0 && AsObject(thing)->IsClosed()) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfStaffRoomsInPark() const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kStaffRoomFlag) != 0) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfClosedStaffRoomsInPark() const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kStaffRoomFlag) != 0 && AsObject(thing)->IsClosed()) count++;
    });
    return count;
}

// The mean filth of the toilets (the signed sum divided by the count), 0 when there is none.
int32_t CParkAnalyser::AvgFilthLevelOfToilets() const {
    int32_t sum = 0;
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) {
            count++;
            sum += FilthOf(AsObject(thing));
        }
    });
    if (count == 0) return 0;
    return sum / count;
}

// The first toilet, or one strictly filthier than the best so far, wins; `filth` is cleared first and written (with the id) only when a toilet was
// found; otherwise the id answered is 0.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CParkAnalyser::FilthiestToilet(int32_t& filth) const {
    uint16_t bestId = 0;
    int32_t best = 0;
    bool found = false;
    filth = 0;
    WalkUsedThings_2(this, [&](uint16_t id, CThing* thing) {
        if (thing->mKind == kObjectKind && (AsObject(thing)->mFlags & kToiletFlag) != 0) {
            const int32_t level = FilthOf(AsObject(thing));
            if (!found || level > best) {
                best = level;
                found = true;
                bestId = id;
            }
        }
    });
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    if (found) {
        filth = best;
        result.mIndex = bestId;
    } else {
        result.mIndex = 0;
    }
    return result;
}

// The rides (UI type 0) of the object list that are broken down.
int32_t CParkAnalyser::NumberOfBrokenDownRidesInPark() const {
    int32_t count = 0;
    WalkObjectList_2(this, [&](CObject* object) {
        const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
        if (info->mUIType == 0 && object->IsBrokenDown()) count++;
    });
    return count;
}

// The rides of the object list whose wear byte (the low byte of the truncated mF40) is below `level` (signed), or that are broken down.
int32_t CParkAnalyser::NumberOfWornRidesInPark(int32_t level) const {
    int32_t count = 0;
    WalkObjectList_2(this, [&](CObject* object) {
        const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType));
        if (info->mUIType == 0) {
            const int32_t wear = ppc_fctiwz(object->mF40) & 0xff;
            if (wear < level || object->IsBrokenDown()) count++;
        }
    });
    return count;
}

// The researched ride types; one with a golden ticket cost (slot 46 not 0) counts only when `includeGoldenTickets` is nonzero (its low byte).
int32_t CParkAnalyser::TypesOfRidesAvailable(bool includeGoldenTickets) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == 0) {
            if (ControlOf(this, info)->mAvailable != 0) {
                if (GoldenTicketCost(info) == 0) count++;
                else if (includeGoldenTickets) count++;
            }
        }
    }
    return count;
}

// The ride types with upgrades available (the argument is not read).
int32_t CParkAnalyser::TypesOfUpgradesAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == 0) {
            if (ControlOf(this, info)->mUpgradesAvailable > 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfShopsAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjShop) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfSideshowsAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjSideshow) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfFeaturesAvailable(bool) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == kUIObjFeature) {
            if (ControlOf(this, info)->mAvailable != 0) count++;
        }
    }
    return count;
}

int32_t CParkAnalyser::TypesOfRidesLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(0)); }
int32_t CParkAnalyser::TypesOfShopsLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(1)); }
int32_t CParkAnalyser::TypesOfSideshowsLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(2)); }
int32_t CParkAnalyser::TypesOfFeaturesLeftToResearch() const { return LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(3)); }

// Nothing left to research (category 5, signed above 0 fails) and every record of a UI type other than 4 has one in the park or a golden ticket cost.
bool CParkAnalyser::AllObjectsResearchedAndPresent() const {
    if (LabOf(this)->GetNumberLeftToResearch(static_cast<ResearchCat>(5)) > 0) return false;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType != kUITypeSkipped) {
            if (ControlOf(this, info)->mNumberInPark == 0 && GoldenTicketCost(info) == 0) return false;
        }
    }
    return true;
}

// Of the records with a golden ticket cost above 0, the share (in percent, truncated) with at least one in the park; 0 when there are none.
int32_t CParkAnalyser::PercentageOfTotalObjectsInPark() const {
    int32_t total = 0;
    int32_t present = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (GoldenTicketCost(info) > 0) {
            total++;
            if (ControlOf(this, info)->mNumberInPark > 0) present++;
        }
    }
    if (total == 0) return 0;
    return present * 100 / total;
}

// The records of that UI type with one or more in the park; a golden ticket one counts only when `includeGoldenTickets` is set.
int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const {
    int32_t count = 0;
    for (const RideInfo* info = gRideDatabase.GetFirstRide(); info != nullptr; info = gRideDatabase.GetNextRide()) {
        if (info->mUIType == type) {
            if (ControlOf(this, info)->mNumberInPark > 0) {
                if (includeGoldenTickets) count++;
                else if (GoldenTicketCost(info) == 0) count++;
            }
        }
    }
    return count;
}

// The largest upgrade byte (+0x4c) of the objects of that type, -1 when there is none.
int32_t CParkAnalyser::GetHighestUpgradeLevelForObject(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    int32_t highest = -1;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind && AsObject(thing)->mObjectType == id.mIndex) {
            const int32_t level = AsObject(thing)->mF4C;
            if (level > highest) highest = level;
        }
    });
    return highest;
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    const CControlManager* manager =
        reinterpret_cast<const CControlManager*>(reinterpret_cast<uintptr_t>(&Savvy(this)->GetWorld()) + 0x1d943c);
    return manager->GetObjectControl(id, 0)->mNumberInPark;
}

// The objects whose record has that UI type; for rides (type 0) only those whose upgrade byte (+0x4c) is at least `level` (signed).
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind) {
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(AsObject(thing)->mObjectType));
            if (type == info->mUIType) {
                if (type != 0 || static_cast<int32_t>(AsObject(thing)->mF4C) >= level) count++;
            }
        }
    });
    return count;
}

int32_t CParkAnalyser::NumberOfClosedObjectTypeInPark(UIObjType type) const {
    int32_t count = 0;
    WalkUsedThings_2(this, [&](uint16_t, CThing* thing) {
        if (thing->mKind == kObjectKind) {
            const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(AsObject(thing)->mObjectType));
            if (type == info->mUIType && AsObject(thing)->IsClosed()) count++;
        }
    });
    return count;
}

// The history of that UI type: its value `back` places back, or id 0 when it has none.
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CParkAnalyser::LastOfObjectTypeResearched(UIObjType type, int32_t back) const {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> out;
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> result;
    if (mLastOfObjectType[type].GetValue(out, back)) result.mIndex = out.mIndex;
    else result.mIndex = 0;
    return result;
}

void CParkAnalyser::SetLastOfObjectTypeResearched(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    mLastOfObjectType[type].SetAtEnd(id);
}

// The index (0 = newest) of the largest value among the entries 0 .. count that the ring holds, only a value above `start` counting (the first of
// equals wins); -1 when none. The loop ends at mNumEntries, after `count`, or at the first index GetValue refuses.
template <class T, int N>
int32_t CHistory<T, N>::GetHighestValue(T start, int32_t count) {
    T best = start;
    int32_t index = -1;
    for (int32_t i = 0; i < mNumEntries && i <= count; ++i) {
        T value;
        if (!GetValue(value, i)) break;
        if (value > best) {   // the float instance: fcmpo, so a NaN never wins
            best = value;
            index = i;
        }
    }
    return index;
}

// How many values the ring holds.
template <class T, int N>
int32_t CHistory<T, N>::HowFarBack() const {
    return mWrappedAround ? mNumEntries : mCurrentEntry + 1;
}

// As GetHighestValue for the smallest value.
template <class T, int N>
int32_t CHistory<T, N>::GetLowestValue(T start, int32_t count) {
    T best = start;
    int32_t index = -1;
    for (int32_t i = 0; i < mNumEntries && i <= count; ++i) {
        T value;
        if (!GetValue(value, i)) break;
        if (value < best) {
            best = value;
            index = i;
        }
    }
    return index;
}

// ---- the three tag_IDX_TYPE default constructors (0x100e2ef0 MapId, 0x100f1d7c ObjectId, 0x10116ccc ThingId) are a lone blr: the tag is an aggregate
// (analyser.h) and its implicit trivial default constructor is the translation.

// ---- the instances the program has (the manifest's addresses)
struct IDXTYPEUniqueNameObjectId;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdTag;
typedef CHistory<ObjectIdTag, 3> HistoryObjectId;

template void CHistory<unsigned char, 50>::SetAtEnd(unsigned char);           // 0x100ef8c4
template void CHistory<int32_t, 50>::SetAtEnd(int32_t);                        // 0x100f0114
template void CHistory<float, 144>::SetAtEnd(float);                           // 0x100f069c
template void CHistory<int32_t, 144>::SetAtEnd(int32_t);                       // 0x100f09dc
template void HistoryObjectId::SetAtEnd(ObjectIdTag);                          // 0x100f0e38
template void CHistory<int32_t, 5>::SetAtEnd(int32_t);                         // 0x100f2944
template void CHistory<int32_t, 30>::SetAtEnd(int32_t);                        // 0x10111d14
template void CHistory<uint32_t, 30>::SetAtEnd(uint32_t);                      // 0x10111e24

template bool HistoryObjectId::GetValue(ObjectIdTag&, int32_t) const;          // 0x100f0f04
template bool CHistory<unsigned char, 50>::GetValue(unsigned char&, int32_t) const;   // 0x100f1dcc
template bool CHistory<int32_t, 50>::GetValue(int32_t&, int32_t) const;        // 0x100f1e68
template bool CHistory<uint32_t, 30>::GetValue(uint32_t&, int32_t) const;      // 0x100f1f0c
template bool CHistory<int32_t, 5>::GetValue(int32_t&, int32_t) const;         // 0x1001bf6c
template bool CHistory<int32_t, 144>::GetValue(int32_t&, int32_t) const;       // 0x1001c2a8
template bool CHistory<int32_t, 30>::GetValue(int32_t&, int32_t) const;        // 0x10111d80
template bool CHistory<float, 144>::GetValue(float&, int32_t) const;           // 0x101f6854

template int32_t CHistory<unsigned char, 50>::HowFarBack() const;              // 0x100efb1c
template int32_t CHistory<int32_t, 50>::HowFarBack() const;                    // 0x100f0370
template int32_t CHistory<uint32_t, 30>::HowFarBack() const;                   // 0x100f1134
template int32_t CHistory<int32_t, 5>::HowFarBack() const;                     // 0x1001c010
template int32_t CHistory<int32_t, 144>::HowFarBack() const;                   // 0x1001c254
template int32_t CHistory<int32_t, 30>::HowFarBack() const;                    // 0x101114c0
template int32_t CHistory<float, 144>::HowFarBack() const;                     // 0x101f69d8

template int32_t CHistory<unsigned char, 50>::GetSumOfLast(double&, int32_t) const;   // 0x100ef9e8
template int32_t CHistory<int32_t, 50>::GetSumOfLast(double&, int32_t) const;  // 0x100f0238
template int32_t CHistory<uint32_t, 30>::GetSumOfLast(double&, int32_t) const; // 0x100f1000
template int32_t CHistory<int32_t, 144>::GetSumOfLast(double&, int32_t) const; // 0x1001c11c
template int32_t CHistory<int32_t, 30>::GetSumOfLast(double&, int32_t) const;  // 0x1011132c

template void CHistory<unsigned char, 50>::GetAverageOfLast(unsigned char&, int32_t) const;   // 0x100ef930
template void CHistory<int32_t, 50>::GetAverageOfLast(int32_t&, int32_t) const;   // 0x100f0180
template void CHistory<int32_t, 144>::GetAverageOfLast(int32_t&, int32_t) const;  // 0x1001c064
template void CHistory<int32_t, 30>::GetAverageOfLast(int32_t&, int32_t) const;   // 0x10111274

template void CHistory<int32_t, 144>::SetFromTemp();                           // 0x100f0a48
template void CHistory<int32_t, 30>::SetFromTemp();                            // 0x10111464
template void CHistory<uint32_t, 30>::SetFromTemp();                           // 0x101117e8

template int32_t CHistory<float, 144>::GetHighestValue(float, int32_t);        // 0x101f68f8
template int32_t CHistory<int32_t, 144>::GetHighestValue(int32_t, int32_t);    // 0x101f6a2c
template int32_t CHistory<int32_t, 144>::GetLowestValue(int32_t, int32_t);     // 0x101f6b0c

// Host only (unit w163): the script blocks of the script system. The Mac allocates a block with LbMem_Alloc(0x118) in SCR_SpawnScript and
// ESP_SpawnScript and frees it with LbMem_Free in SCR_KillScript. The table's words are Ptr32s (see src/ScriptTick.cpp), so a block has to
// come from the arena. Unit libraries never link this file: they fake the pair with the events the Mac's allocator calls would log.

// The Mac's LbMem_Alloc(0x118): contents undefined, nullptr when the arena is used up.
tagSCRIPT* SCR_NewScriptBlock() { return static_cast<tagSCRIPT*>(tpw::ArenaAlloc(sizeof(tagSCRIPT))); }

// The Mac's LbMem_Free of a script block.
void SCR_FreeScriptBlock(tagSCRIPT* script) { tpw::ArenaFree(script); }

// The script sprites' host records (host only, no Mac code; unit v5L05, 2026-10-20).
//
// A script block (tagSCRIPT, 0x118 bytes) keeps the sprite it draws as an SSpriteGameData embedded at +0xd0 in the Mac's
// layout (0x44 bytes, 32-bit pointers): the block is the save file's record, so it keeps that layout. ESP_AddHLSpriteFromScript
// fills those bytes as the Mac does, then lists the script's host record from here instead of the embedded one: the sprite
// engine reads the host's SSpriteGameData, whose pointers are 8 bytes (mSprite at 0x28..0x2f, mF2C at 0x30), so the embedded
// bytes read as one would make mSprite of the Mac's mSprite and mF2C together. SCR_WhichScriptHasThisSprite matches the
// host record too (the hot sprite the peep picker asks about is the one that was drawn).

namespace {

// Keyed by the block's address; a block freed and allocated again at the same address gets the same record, which the adder
// fills again before listing it. The records are never freed (one for each block address the game has used).
std::unordered_map<const tagSCRIPT*, SSpriteGameData*>& Table() {
    static std::unordered_map<const tagSCRIPT*, SSpriteGameData*> table;
    return table;
}

}  // namespace

SSpriteGameData* ESP_ScriptHostSprite(const tagSCRIPT* script) {
    SSpriteGameData*& sprite = Table()[script];
    if (sprite == nullptr) sprite = static_cast<SSpriteGameData*>(std::calloc(1, sizeof(SSpriteGameData)));
    return sprite;
}

bool ESP_IsScriptHostSprite(const tagSCRIPT* script, const void* sprite) {
    const auto& table = Table();
    const auto found = table.find(script);
    return found != table.end() && found->second == sprite;
}

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

// Unit w146. The one-byte and two-byte instances MapDelta.cpp's serialisers use: LOADVAR and SAVEVAR of unsigned char (0x100f17c8, 228 bytes;
// 0x100f1a04, 236 bytes) and short (0x100f518c, 236 bytes; 0x100f5658, 252 bytes). The shared bodies are the same as the others': the listings
// differ only in the byte size, the swap (a byte is not swapped) and the order of the two asserts.
template<> int32_t LOADVAR<uint8_t>(void* file, uint8_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f17c8, 228 bytes
}
template<> int32_t SAVEVAR<uint8_t>(void* file, uint8_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f1a04, 236 bytes
}
// The object id and the map id (halfwords in structs, swapped like the thing id: lhz; sthbrx / the rlwinm pair) and TbTimeStamp. The Mac keeps a
// TbTimeStamp big-endian (high word at +0) and the file holds the 8 bytes little-endian: the listings reverse the whole value through the shift
// helpers 0x10237f50 and 0x10237f78 (masks 0xff0000, 0xff000000, 0xff00, 0xff on each half), stored as stw low word at +4, high word at +0; here
// serialise::SwapWord reverses the 8 bytes (WordOf<8> in Serialise.h).
namespace {
using ObjectIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
}
static void SwapForFile(ObjectIdStruct& id) { id.mIndex = serialise::SwapWord(id.mIndex); }
static void UnswapForFile(ObjectIdStruct& id) { id.mIndex = serialise::UnswapWord(id.mIndex); }
static void SwapForFile(TbTimeStamp& stamp) { stamp.mTicks = serialise::SwapWord(stamp.mTicks); }
static void UnswapForFile(TbTimeStamp& stamp) { stamp.mTicks = serialise::UnswapWord(stamp.mTicks); }

template<> int32_t LOADVARSTRUCT<ObjectIdStruct>(void* file, ObjectIdStruct* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f1300, 224 bytes
}
template<> int32_t SAVEVARSTRUCT<ObjectIdStruct>(void* file, ObjectIdStruct* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f1188, 240 bytes
}
template<> int32_t LOADVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f1468, 516 bytes
}
template<> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f1b1c, 536 bytes
}

// The serialisation instances for float (unit w114): LOADVAR<float> 0x100f16b4 (236 bytes) and SAVEVAR<float> 0x100f18d8 (260 bytes).
// Each is one explicit specialisation over the shared bodies in Serialise.h, so it is a separate symbol as the original's. The four
// bytes are moved and swapped as a word and never converted, so a float keeps its bits (the original uses integer loads and byte
// reversing stores in the loader, and an lfs/stfs pair in the saver that copies the word as it is).
//
// Quirks kept: as for the other 4-byte instances (src/Serialise_basic.cpp): a failed read or write leaves the value alone and answers 1;
// the loader's two asserts come before the swap and the saver's before the write.

template<> int32_t LOADVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f16b4, 236 bytes
}

template<> int32_t SAVEVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f18d8, 260 bytes
}
