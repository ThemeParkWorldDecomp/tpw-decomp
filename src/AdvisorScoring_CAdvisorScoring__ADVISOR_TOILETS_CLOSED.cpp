// AdvisorScoring_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.cpp (main app of SimTheme Park 2.04b3): eight of CAdvisorScoring's scorers
// (unit v5040), kept apart from src/AdvisorScoring.cpp so the unit libraries that compile that file need none of their callees.
#include "AdvisorScoring.h"

// Unit v5040: eight scorers. Mac offset o of the scoring object holds balance slot (o - 0x14) / 4 (the table's values start at 0x14).
#include "MainBalance.h"
#include "ObjectParts.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

uint16_t gSideshowNoneId = 0;   // 0x1027df2c (initial word 0): the "no thing" id GetSideshowWithBestSatisfaction answers

namespace {
int32_t ScoringSlot(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}
}  // namespace

// 0x1001aa20.
int32_t CAdvisorScoring::_ADVISOR_TOILETS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfToiletsInPark();
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedToiletsInPark();
    if (closed * 100 / total > ScoringSlot(this, 0x598)) return ScoringSlot(this, 0x59c);
    return 0;
}

// 0x1001aafc.
int32_t CAdvisorScoring::_ADVISOR_STAFFROOM_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfStaffRoomsInPark();
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedStaffRoomsInPark();
    if (closed * 100 / total > ScoringSlot(this, 0x5a0)) return ScoringSlot(this, 0x5a4);
    return 0;
}

// 0x1001a934: sideshows are UI type 2.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(2), 0);
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(2));
    if (closed * 100 / total > ScoringSlot(this, 0x590)) return ScoringSlot(this, 0x594);
    return 0;
}

// 0x1001abdc.
int32_t CAdvisorScoring::_ADVISOR_LOTS_OF_PRANKS() {
    const int32_t pranks = gGameWorldRef->GetParkAnalyser()->MisbehavingVisitorsLastMonth(false);
    if (pranks > ScoringSlot(this, 0x5a8)) return ScoringSlot(this, 0x5ac);
    return 0;
}

// 0x1001a84c: shops are UI type 1.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(1), 0);
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(1));
    if (closed * 100 / total > ScoringSlot(this, 0x588)) return ScoringSlot(this, 0x58c);
    return 0;
}

// 0x1001a728: rides are UI type 0; only with the gates open and at least the minimum number of rides.
int32_t CAdvisorScoring::_ADVISOR_RIDES_CLOSED() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 0);
    if (total == 0) return 0;
    if (total < ScoringSlot(this, 0x57c)) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(0));
    if (closed * 100 / total > ScoringSlot(this, 0x580)) return ScoringSlot(this, 0x584);
    return 0;
}

// 0x1001a5e4: records the best sideshow in the object (halfword 4, word 8) when its satisfaction is above the limit.
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t satisfaction = gGameWorldRef->GetParkAnalyser()->GetSideshowWithBestSatisfaction(id);
    if (id.mIndex == gSideshowNoneId) return 0;
    const int32_t limit = ScoringSlot(this, 0x570);
    if (satisfaction > limit) {
        mWord4 = id.mIndex;
        mWord8 = static_cast<uint32_t>(satisfaction);
        return ScoringSlot(this, 0x574) * (satisfaction - limit);
    }
    return 0;
}

// 0x1001aef8: no points once a ticket is held; else visitors above (float)balance[0x750] * factor (truncated).
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE() {
    CGoldenTicketControl* control = reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<char*>(gGameWorld) + 0x2c0);
    if (control->HowManyTickets() > 0) return 0;
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
    const float scaled = static_cast<float>(gMainBalance.mValues[(0x750 - 8) / 4].mInteger) * mBalance.mValues[(0x5b8 - 0x14) / 4].mFloat;
    if (visitors > ppc_fctiwz(static_cast<double>(scaled))) return ScoringSlot(this, 0x5bc);
    return 0;
}
