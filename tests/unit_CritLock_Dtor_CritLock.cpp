// Test library for CritLock::~CritLock (unit v5732; libtpw_unit_CritLock_Dtor_CritLock.dylib), main app:
//   src/..._CritLock_Dtor_CritLock.cpp   CritLock_DtorBody
//
// LeaveCriticalSection and operator delete are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbSCBack.h"
#include "platform/macdoze.h"

using namespace calllog;

void LbSE_OperatorDelete(void* block) { ev("delete", {lab(block)}); }

namespace NS_MacDoze {
void LeaveCriticalSection(void** section) { ev("Leave", {lab(section)}); }
}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_crit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_crit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CritLock, mSection));
    return n;
}

uint64_t tpw_crit_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        CritLock_DtorBody(reinterpret_cast<CritLock*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag))));
}

}  // extern "C"
