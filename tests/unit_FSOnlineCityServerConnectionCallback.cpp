// Test library for FSOnlineCityServerConnectionCallback (unit v5271; libtpw_unit_FSOnlineCityServerConnectionCallback.dylib), main app:
//   src/game/online/FS_Online.cpp
//
// Fakes: CFSOnlineSystem::CFSOnlineSystem and CFSOnlineSystem::CityServerConnectionCallback record their events. gOnlineSystem is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

CFSOnlineSystem::CFSOnlineSystem() {
    ev("ctor", {newlab(this)});
}

void CFSOnlineSystem::CityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    ev("Callback", {lab(this), a, b, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(c))),
                    static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(d)))});
}

extern "C" {

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_cs_set(uint32_t existing) {
    gOnlineSystem = existing ? reinterpret_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(16)) : nullptr;
}

uint32_t tpw_cs_get() { return gOnlineSystem != nullptr; }

void tpw_cs_call(int32_t a, int32_t b, uint32_t c, uint32_t d) {
    FSOnlineCityServerConnectionCallback(a, b, reinterpret_cast<void*>(static_cast<uintptr_t>(c)),
                                         reinterpret_cast<void*>(static_cast<uintptr_t>(d)));
}

}  // extern "C"
