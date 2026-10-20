// Test library for TestAdvisor2/3/4 and delete_ride (unit v5111; libtpw_unit_TestAdvisor3.dylib, tests/test_TestAdvisor3.py), main app:
//   src/turn_object_counterclockwise_TestAdvisor3.cpp
//
// Callees ADV_Stat, ADV_Call, LLCC_SystemSetMode, LLCM_LeftMouseButtonUp are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "Advisor.h"
#include "LLCellSystem.h"
#include "intrface.h"

using namespace calllog;

int32_t gTestAdvisorIndex;
CIntrface gIntrface;

int32_t ADV_Stat() {
    ev("ADV_Stat", {0});
    return static_cast<int32_t>(ret("ADV_Stat"));
}

int32_t ADV_Call(AdvisorResponse response) {
    ev("ADV_Call", {response.mId});
    return 0;
}

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag ? 1 : 0}); }

void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag});
}

extern "C" {

uint64_t tpw_ta_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"index", &gTestAdvisorIndex},
        {"intrface", &gIntrface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ta_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ta_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    return n;
}

uint32_t tpw_ta_ta2() { return static_cast<uint32_t>(TestAdvisor2()); }
uint32_t tpw_ta_ta3() { return static_cast<uint32_t>(TestAdvisor3()); }
uint32_t tpw_ta_ta4() { return static_cast<uint32_t>(TestAdvisor4()); }
uint32_t tpw_ta_del() { return static_cast<uint32_t>(delete_ride()); }

}  // extern "C"
