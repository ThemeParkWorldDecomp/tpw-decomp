// Test library for the eight _ADVISOR_MAX_* scorers (unit v5108; libtpw_unit_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2.dylib), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// Callees outside the unit: CWorld::GetParkAnalyser and CParkAnalyser::NumberOfStaffType, fakes that record the arguments the real ones read and
// answer from the test's queue. gGameWorld and gMainBalance (other files' globals) are defined here; the balance table's reader is faked empty
// (the test writes the slots itself).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "MainBalance.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files
CWorld* gGameWorld = nullptr;
void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance gMainBalance;

namespace {
alignas(16) unsigned char gAnalyserBlock[64];
}

// ---- the fakes
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock);
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int64_t>(static_cast<int32_t>(model))});
    return static_cast<int32_t>(ret("Staff"));
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"values", &gMainBalance.mValues[0]},
        {"analyser", gAnalyserBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offset of the advisor's first balance value, the byte size of a value slot, the size of the scoring object.
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    alignas(16) static unsigned char buf[sizeof(CAdvisorScoring)];
    CAdvisorScoring* p = reinterpret_cast<CAdvisorScoring*>(buf);
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&p->mBalance.mValues[0]) - buf);
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    return n;
}

int32_t tpw_adv_call(int32_t which, uint64_t object) {
    CAdvisorScoring* self = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (which) {
        case 0: return self->_ADVISOR_MAX_HANDYMEN_V2();
        case 1: return self->_ADVISOR_MAX_ENTERTAINERS();
        case 2: return self->_ADVISOR_MAX_ENTERTAINERS_V2();
        case 3: return self->_ADVISOR_MAX_MECHANICS_V2();
        case 4: return self->_ADVISOR_MAX_GUARDS();
        case 5: return self->_ADVISOR_MAX_GUARDS_V2();
        case 6: return self->_ADVISOR_MAX_RESEARCHERS();
        case 8: return self->_ADVISOR_MAX_MECHANICS();
        case 9: return self->_ADVISOR_BOAT_ARRIVED();
        default: return self->_ADVISOR_MAX_RESEARCHERS_V2();
    }
}

}  // extern "C"
