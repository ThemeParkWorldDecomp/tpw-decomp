// Test library for unit v51116 (libtpw_unit_CAdvisorScoring__ADVISOR_AWARD_MOVED.dylib, tests/test_CAdvisorScoring__ADVISOR_AWARD_MOVED.py), main app:
//   src/AdvisorScoring_..._AWARD_MOVED.cpp   the 8 byte scorers, ~CAdvisorBalance, CMsgTag, the inline byte swaps.
//
// Callees outside the unit, recording fakes with the same events as the PowerPC stubs: operator delete, CBalanceTable::UnInit,
// SamsUtilities::_USwapBlock32.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "AdvisorScoring.h"

using namespace calllog;

namespace v51116_callee {
void OperatorDelete(void* block) { ev("OperatorDelete", {lab(block)}); }
}  // namespace v51116_callee

void CBalanceTable::UnInit() { ev("UnInit", {lab(this)}); }

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) { ev("USwapBlock32", {lab(words), count}); }
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_v51116_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// size of the scoring object, offset of balance slot 0, stride of a slot
int32_t tpw_v51116_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mBalance) + offsetof(CAdvisorBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

uint32_t tpw_v51116_score(int32_t which, uint64_t obj) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(obj));
    switch (which) {
        case 0: return s->_ADVISOR_AWARD_MOVED();
        case 1: return s->_ADVISOR_AWARD_GIVEN();
        case 2: return s->_ADVISOR_GENERAL_WON_GOLDEN_TICKET();
        case 3: return s->_ADVISOR_RESEARCH_FEATURE_RESEARCHED();
        case 4: return s->_ADVISOR_RESEARCH_SIDESHOW_RESEARCHED();
        case 5: return s->_ADVISOR_RESEARCH_SHOP_RESEARCHED();
        case 6: return s->_ADVISOR_RESEARCH_ADDON_RESEARCHED();
        case 7: return s->_ADVISOR_RESEARCH_RIDE_RESEARCHED();
        case 8: return s->_ADVISOR_FEATURES_BUILD_REST_AREA();
        case 9: return s->_ADVISOR_FEATURES_FAR_TO_TOILETS();
        case 10: return s->_ADVISOR_RIDES_BREAKDOWN_MECHANIC_ON_IT();
        case 11: return s->_ADVISOR_STAFF_GUARD_COVERAGE_LOST_WORLD();
        case 12: return s->_ADVISOR_STAFF_GUARD_COVERAGE();
        case 13: return s->_ADVISOR_WELCOME();
        default: return s->_ADVISOR_NO_SCORE();
    }
}

uint64_t tpw_v51116_bal_dtor(uint64_t obj, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(CAdvisorBalanceDestruct(reinterpret_cast<CAdvisorBalance*>(static_cast<uintptr_t>(obj)), flag)));
}

uint64_t tpw_v51116_msg_dtor(uint64_t obj, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(CMsgTagDestruct(reinterpret_cast<CMsgTag*>(static_cast<uintptr_t>(obj)), flag)));
}

int32_t tpw_v51116_msg_type(uint64_t obj) { return reinterpret_cast<const CMsgTag*>(static_cast<uintptr_t>(obj))->GetType(); }

uint32_t tpw_v51116_swap32(int32_t which, uint32_t v) {
    using namespace v51116_swap;
    auto in = [&](auto* tag) { return reinterpret_cast<decltype(tag)>(static_cast<uintptr_t>(v)); };
    auto out = [](const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); };
    switch (which) {
        case 0: return out(USwap32_tagBUMPRIDE(in(static_cast<tagBUMPRIDE*>(nullptr))));
        case 1: return out(USwap32_tabCOLOBJECT(in(static_cast<tabCOLOBJECT*>(nullptr))));
        case 2: return out(USwap32_tagBUMP(in(static_cast<tagBUMP*>(nullptr))));
        case 3: return out(USwap32_tagPEEPOFF(in(static_cast<tagPEEPOFF*>(nullptr))));
        default: return out(USwap32_long_ptr(in(static_cast<int32_t*>(nullptr))));
    }
}

void tpw_v51116_inplace(int32_t which, uint64_t p) {
    using namespace v51116_swap;
    void* q = reinterpret_cast<void*>(static_cast<uintptr_t>(p));
    switch (which) {
        case 0: USwapInplace32_long_10021db8(*static_cast<int32_t*>(q)); break;
        case 1: USwapInplace32_ulong_10021dfc(*static_cast<uint32_t*>(q)); break;
        case 2: USwapInplace32_long_10009c74(*static_cast<int32_t*>(q)); break;
        case 3: USwapInplace32_int_10009c30(*static_cast<int32_t*>(q)); break;
        default: USwapInplace32_ulong_10009b2c(*static_cast<uint32_t*>(q)); break;
    }
}

void tpw_v51116_block(int32_t which, uint64_t p, int32_t count) {
    void* q = reinterpret_cast<void*>(static_cast<uintptr_t>(p));
    if (which == 0) v51116_swap::USwapBlock32_ulong(static_cast<uint32_t*>(q), count);
    else v51116_swap::USwapBlock32_long(static_cast<int32_t*>(q), count);
}

}  // extern "C"
