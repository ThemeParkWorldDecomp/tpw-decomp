// Test library for ChatCallbackFn (unit v5212; libtpw_unit_ChatCallbackFn.dylib, tests/test_ChatCallbackFn.py), main app:
//   src/COnlineControl_..._ChatCallbackFn.cpp   ChatCallbackFn
//
// The callees (operator new, the constructor, the mutex, AddUIError, COnlineControl::CallbackFn) are fakes recording what they read; pointers
// to the control are logged as "is it gOnlineControl". gOnlineControl is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "UI_pump.h"
#include "online.h"

using namespace calllog;

COnlineControl* gOnlineControl;

namespace {
int64_t Same(const void* p, ptrdiff_t delta) {
    return reinterpret_cast<const char*>(p) - delta == reinterpret_cast<const char*>(gOnlineControl) ? 1 : 0;
}
}  // namespace

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace online_callee

COnlineControl::COnlineControl() { ev("ctor", {}); }

void bthread_mutex::lock() { ev("lock", {Same(this, 0x1c)}); }
void bthread_mutex::unlock() { ev("unlock", {Same(this, 0x1c)}); }

void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {static_cast<int64_t>(error), table != nullptr ? 1 : 0}); }

void COnlineControl::CallbackFn(int32_t id, void* arguments) {
    ev("CallbackFn", {Same(this, 0), static_cast<int64_t>(id), static_cast<int64_t>(*static_cast<uint32_t*>(arguments))});
}

extern "C" {

uint64_t tpw_cco_addr(const char* name) {
    if (std::strcmp(name, "gOnline") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineControl));
    return 0;
}

uint64_t tpw_cco_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cco_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineControl, mClientReady));
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    return n;
}

void tpw_cco_a(int32_t id, uint64_t arguments) { ChatCallbackFn(id, nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(arguments))); }

}  // extern "C"
