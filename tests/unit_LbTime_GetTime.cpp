// Test library for LbTime_GetTime and LbTime_GetDate (unit v5309; libtpw_unit_LbTime_GetTime.dylib), main app:
//   src/bullfrog/LbTime.cpp
//
// NS_MacDoze::GetLocalTime is a fake: it writes eight 16-bit words taken from the case's queues "st:0".."st:7".
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbTime.h"
#include "platform/macdoze.h"

using namespace calllog;

namespace NS_MacDoze {
void GetLocalTime(_SYSTEMTIME* st) {
    ev("GetLocalTime", {});
    uint16_t* w = reinterpret_cast<uint16_t*>(st);
    static const char* const keys[8] = {"st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7"};
    for (int i = 0; i < 8; i++) w[i] = static_cast<uint16_t>(ret(keys[i]));
}
}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_lbt_addr(const char*) { return 0; }

uint64_t tpw_lbt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lbt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbTime));
    out[n++] = static_cast<int64_t>(offsetof(TbDate, mDayOfWeek));
    return n;
}

void tpw_lbt_time(uint64_t p) { LbTime_GetTime(reinterpret_cast<TbTime*>(static_cast<uintptr_t>(p))); }
void tpw_lbt_date(uint64_t p) { LbTime_GetDate(reinterpret_cast<TbDate*>(static_cast<uintptr_t>(p))); }

}  // extern "C"
