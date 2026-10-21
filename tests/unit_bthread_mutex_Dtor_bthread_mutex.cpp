// Test library for bthread_mutex::~bthread_mutex (unit v5120; libtpw_unit_bthread_mutex_Dtor_bthread_mutex.dylib), main app:
//   src/game/online/email.cpp   bthread_mutex::Destroy
//
// DeleteCriticalSection and operator delete are recording fakes (the delete does not free: the blocks come from the arena).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "UI_pump.h"
#include "online.h"

using namespace calllog;

namespace NS_MacDoze {
void DeleteCriticalSection(void** handle) {
    ev("DeleteCriticalSection", {lab(handle)});
}
}  // namespace NS_MacDoze

namespace online_callee {
void OperatorDelete(void* p) {
    ev("delete", {lab(p)});
}
}  // namespace online_callee

extern "C" {

uint64_t tpw_mtxd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_mtxd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(bthread_mutex, mHandle));
    return n;
}

uint64_t tpw_mtxd_destroy(uint64_t self, int16_t flag) {
    bthread_mutex* r = bthread_mutex::Destroy(reinterpret_cast<bthread_mutex*>(static_cast<uintptr_t>(self)), flag);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
