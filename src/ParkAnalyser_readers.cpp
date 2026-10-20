// The park analyser's readers of the things (main app, analyser.cpp range; unit w125): CParkAnalyser::HowBusyIsPark 0x100e5944 (420 bytes),
// NumberOfVisitorsInPark 0x100e7278 (240 bytes) and CountNeighboursOfType 0x100e6a6c (424 bytes).
//
// Each walk asserts, once per thing it looks at, that entry 0 of the thing table is free (the text at 0x10240242, the same one World_turn.cpp
// uses); the release GAME_ASSERTMSG does nothing, so the asserts only show in a call log. A thing is reached through gThingArray[id].mThing (a NULL
// one would crash the Mac as it does here). The end of a list is the id read from a 16-bit word in initialised data (0x102a045a for HowBusyIsPark,
// 0x102a04c6 for NumberOfVisitorsInPark, 0x102a04c2 for CountNeighboursOfType), all 0 and never written.
#include <cstdint>

#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

namespace {

constexpr uint16_t kNoThing = 0;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242

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

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    return CountPeopleInsidePark(this);
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
