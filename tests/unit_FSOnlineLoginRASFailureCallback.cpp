// Test library for the online login callbacks (unit v5113; libtpw_unit_FSOnlineLoginRASFailureCallback.dylib), main app:
//   src/game/online/FS_Online.cpp   FSOnlineLoginRASFailureCallback, FSOnlineLoginConnectionCallback
//
// The constructor and the two forwarded calls are recording fakes; gOnlineSystem is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

CFSOnlineSystem::CFSOnlineSystem() { ev("ctor", {newlab(this)}); }

void CFSOnlineSystem::LoginRASCancelled() { ev("LoginRASCancelled", {lab(this)}); }

void CFSOnlineSystem::LoginConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    ev("LoginConnectionCallback", {lab(this), a, b, lab(c), lab(d)});
}

extern "C" {

uint64_t tpw_frc_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineSystem));
    return 0;
}

uint64_t tpw_frc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_frc_layout(int64_t*) { return 0; }

void tpw_frc_ras() { FSOnlineLoginRASFailureCallback(); }

void tpw_frc_conn(int32_t a, int32_t b, uint64_t c, uint64_t d) {
    FSOnlineLoginConnectionCallback(a, b, reinterpret_cast<void*>(static_cast<uintptr_t>(c)), reinterpret_cast<void*>(static_cast<uintptr_t>(d)));
}

}  // extern "C"
