// Test library for twelve advisor scorers (unit v5892; libtpw_unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.dylib,
// tests/test_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.py), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The callees outside the unit (the world's analyser and staff HQ getters, the analyser's readers, the master save's constructor and
// HaveTicket, operator new) are recording fakes that answer from the test's queues. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "FS_MasterSave.h"
#include "MainBalance.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CFSGlobalMasterSave* gMasterSave;
// The main balance table: only two integer slots are read, so raw storage under the program's symbol (no constructor, no Init).
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

namespace {

constexpr uintptr_t kAnalyser = 0x10381000;
constexpr uintptr_t kWorld = 0x10381200;
constexpr uintptr_t kMaster = 0x10381100;

CStaffHQ* gHq;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FloatRet(const char* key) {
    const uint32_t bits = static_cast<uint32_t>(ret(key));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

bool gInNew = false;

}  // namespace

// ---- operator new, logged like the PowerPC fake (the unit's only allocation is the master save)

// ---- the fakes (same event names and arguments as the module's PowerPC stubs)
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(kAnalyser);
}
CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return gHq;
}
float CParkAnalyser::GetHappinessLevel(uint32_t a) const {
    ev("GetHappinessLevel", {lab(this), static_cast<int64_t>(static_cast<int32_t>(a))});
    return FloatRet("GetHappinessLevel");
}
float CParkAnalyser::GetStaffHappinessLevel(uint32_t a) const {
    ev("GetStaffHappinessLevel", {lab(this), static_cast<int64_t>(static_cast<int32_t>(a))});
    return FloatRet("GetStaffHappinessLevel");
}
int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfVisitorsInPark"));
}
int32_t CParkAnalyser::NumberOfVisitorsWithBalloons() const {
    ev("NumberOfVisitorsWithBalloons", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfVisitorsWithBalloons"));
}
int32_t CParkAnalyser::GetParkValue(uint32_t a) const {
    ev("GetParkValue", {lab(this), static_cast<int64_t>(static_cast<int32_t>(a))});
    return static_cast<int32_t>(ret("GetParkValue"));
}
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel m) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int64_t>(static_cast<int32_t>(m))});
    return static_cast<int32_t>(ret("NumberOfStaffType"));
}
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel m) const {
    ev("AverageQualityOfStaffType", {lab(this), static_cast<int64_t>(static_cast<int32_t>(m))});
    return static_cast<int32_t>(ret("AverageQualityOfStaffType"));
}
int32_t CParkAnalyser::NumberOfStaff() const {
    ev("NumberOfStaff", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfStaff"));
}
bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return ret("GatesOpen") != 0;
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType t, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), static_cast<int64_t>(static_cast<int32_t>(t)), static_cast<int64_t>(level)});
    return static_cast<int32_t>(ret("NumberOfObjectTypeInPark"));
}
int32_t CParkAnalyser::GetAverageExcitementLevelOfAllRides(uint32_t a) const {
    ev("GetAverageExcitementLevelOfAllRides", {lab(this), static_cast<int64_t>(static_cast<int32_t>(a))});
    return static_cast<int32_t>(ret("GetAverageExcitementLevelOfAllRides"));
}
uint8_t CParkAnalyser::GetScariestRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("GetScariestRide", {lab(this)});
    id.mIndex = static_cast<uint16_t>(ret("GetScariestRide"));
    return 0;
}
// The members of the master save are destroyed on the constructor's unwind path; the program's definition is in a file this library lacks.
template <> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {lab(this)}); }
bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal t) const {
    ev("HaveTicket", {lab(this), static_cast<int64_t>(static_cast<int32_t>(t))});
    return ret("HaveTicket") != 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameWorld", &gGameWorld},
        {"gMasterSave", &gMasterSave},
        {"hq", &gHq},
        {"mainbal467", &reinterpret_cast<CMainBalance*>(gMainBalanceStorage)->mValues[467]},
        {"mainbal468", &reinterpret_cast<CMainBalance*>(gMainBalanceStorage)->mValues[468]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Host sizes and addresses for the Mac offsets the module writes.
uint32_t tpw_adv_objsize() { return static_cast<uint32_t>(sizeof(CAdvisorScoring)); }
uint32_t tpw_adv_hqsize() { return static_cast<uint32_t>(sizeof(CStaffHQ)); }
uint64_t tpw_adv_slot(uint64_t obj, int32_t macOffset) {
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(obj));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&o->mBalance.mValues[(macOffset - 0x14) / 4]));
}
uint64_t tpw_adv_word4(uint64_t obj) {
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(obj));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&o->mWord4));
}
uint64_t tpw_adv_hqword(uint64_t hq, int32_t i) {
    CStaffHQ* h = reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&h->mTraining.mF00[i]));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(uint16_t));
    return n;
}

// ---- the functions: 0 .. 11 in the module's order
uint32_t tpw_adv_call(int32_t which, uint64_t obj) {
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(obj));
    switch (which) {
        case 0: return static_cast<uint32_t>(o->_ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS());
        case 1: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_BALLOONS());
        case 2: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_SEND_POSTCARD());
        case 3: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_PUBLISH_PARK());
        case 4: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_MECHANICS_TRAINED());
        case 5: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_ENTERTAINERS_TRAINED());
        case 6: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_HANDYMEN_TRAINED());
        case 7: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_RESEARCHERS_TRAINED());
        case 8: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_SECURITY_TRAINED());
        case 9: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_VISITORS_HAPPY());
        case 10: return static_cast<uint32_t>(o->_ADVISOR_CONGRAT_STAFF_HAPPY());
        default: return static_cast<uint32_t>(o->_ADVISOR_VISITORS_TOO_SCARED());
    }
}

}  // extern "C"
