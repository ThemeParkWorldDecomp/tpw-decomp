// Test library for eight advisor scorers (unit v5040; libtpw_unit_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.dylib,
// tests/test_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.py), main app:
//   src/AdvisorScoring.cpp   the _ADVISOR_* scorers at 0x1001a5e4..0x1001aef8 and gSideshowNoneId
//
// The callees (CWorld::GetParkAnalyser and the CParkAnalyser and CGoldenTicketControl queries) are fakes that record the arguments the real
// ones read and answer from the test's queues. gGameWorld, gGameWorldRef and gMainBalance (raw storage, no constructor) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "World.h"
#include "analyser.h"
#include "ObjectParts.h"
#include "MainBalance.h"
#include "Debug.h"
#include "FS_Online.h"

using namespace calllog;

extern uint16_t gSideshowNoneId;

CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

namespace {
CParkAnalyser* gAnalyser = nullptr;
}

// ---- the rest of the file's link needs (the untouched queries and the constructor are never run here)
AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];
CGameType::CGameType() {}
void CBalanceTable::Init() {}
int32_t CBalanceTable::Load(const char*, LoadMode) { return 1; }
void CBalanceTable::UnInit() {}
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() { return mValues; }

// ---- the fakes
void CTRACE(uint32_t, const char*, ...) {}   // only the untouched queries of this file trace
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyser;
}
int32_t CParkAnalyser::NumberOfToiletsInPark() const {
    ev("NumberOfToiletsInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfToiletsInPark"));
}
int32_t CParkAnalyser::NumberOfClosedToiletsInPark() const {
    ev("NumberOfClosedToiletsInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfClosedToiletsInPark"));
}
int32_t CParkAnalyser::NumberOfStaffRoomsInPark() const {
    ev("NumberOfStaffRoomsInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfStaffRoomsInPark"));
}
int32_t CParkAnalyser::NumberOfClosedStaffRoomsInPark() const {
    ev("NumberOfClosedStaffRoomsInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfClosedStaffRoomsInPark"));
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), static_cast<int32_t>(type), level});
    return static_cast<int32_t>(ret("NumberOfObjectTypeInPark"));
}
int32_t CParkAnalyser::NumberOfClosedObjectTypeInPark(UIObjType type) const {
    ev("NumberOfClosedObjectTypeInPark", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("NumberOfClosedObjectTypeInPark"));
}
bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return ret("GatesOpen") != 0;
}
int32_t CParkAnalyser::MisbehavingVisitorsLastMonth(bool gotAway) const {
    ev("MisbehavingVisitorsLastMonth", {lab(this), gotAway ? 1 : 0});
    return static_cast<int32_t>(ret("MisbehavingVisitorsLastMonth"));
}
int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("NumberOfVisitorsSinceBeginning", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfVisitorsSinceBeginning"));
}
int32_t CParkAnalyser::GetSideshowWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("GetSideshowWithBestSatisfaction", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("BestId"));
    return static_cast<int32_t>(ret("GetSideshowWithBestSatisfaction"));
}
int32_t CGoldenTicketControl::HowManyTickets() const {
    ev("HowManyTickets", {lab(this)});
    return static_cast<int32_t>(ret("HowManyTickets"));
}

extern "C" {

uint64_t tpw_cadv_addr(const char* name) {
    if (std::strcmp(name, "none") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSideshowNoneId));
    return 0;
}

uint64_t tpw_cadv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world and analyser the scorers reach (gGameWorld, the analyser the GetParkAnalyser fake answers).
void tpw_cadv_set_world(uint64_t world, uint64_t analyser) {
    gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
}

// gMainBalance's value at the Mac offset 0x750.
void tpw_cadv_set_main(int32_t value) {
    reinterpret_cast<CMainBalance*>(gMainBalanceStorage)->mValues[(0x750 - 8) / 4].mInteger = value;
}

// The host offsets of the scoring object: mWord4, mWord8, the first balance value, sizeof.
int32_t tpw_cadv_layout(int64_t* out) {
    alignas(16) static unsigned char buf[sizeof(CAdvisorScoring)];
    CAdvisorScoring* p = reinterpret_cast<CAdvisorScoring*>(buf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<unsigned char*>(&p->mWord4) - buf;
    out[n++] = reinterpret_cast<unsigned char*>(&p->mWord8) - buf;
    out[n++] = reinterpret_cast<unsigned char*>(&p->mBalance.mValues[0]) - buf;
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    return n;
}

int32_t tpw_cadv_call(int32_t which, uint64_t object) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (which) {
        case 0: return s->_ADVISOR_TOILETS_CLOSED();
        case 1: return s->_ADVISOR_STAFFROOM_CLOSED();
        case 2: return s->_ADVISOR_SIDESHOWS_CLOSED();
        case 3: return s->_ADVISOR_LOTS_OF_PRANKS();
        case 4: return s->_ADVISOR_SHOPS_CLOSED();
        case 5: return s->_ADVISOR_RIDES_CLOSED();
        case 6: return s->_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION();
        default: return s->_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE();
    }
}

}  // extern "C"
