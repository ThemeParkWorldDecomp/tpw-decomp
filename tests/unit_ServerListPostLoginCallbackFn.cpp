// Test library for ServerListPostLoginCallbackFn (unit v5721; libtpw_unit_ServerListPostLoginCallbackFn.dylib), main app:
//   src/ServerListPostLoginCallbackFn.cpp
//
// The callees outside the unit (the CFSOnlineSystem constructor and the two server list functions) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

static int32_t Lst(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }

// ---- the fakes
CFSOnlineSystem::CFSOnlineSystem() {
    uintptr_t v = static_cast<uintptr_t>(ret("NewList") & 0xffffffffu);
    mServerList = reinterpret_cast<CFSOnlineSystemServerList*>(v);
    ev("Ctor", {});
}

template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::LoggedInGetList() {
    ev("LoggedInGetList", {Lst(this)});
}

template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::CompleteGetList(int32_t error) {
    ev("CompleteGetList", {Lst(this), error});
}

extern "C" {

uint64_t tpw_slp_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineSystem));
    return 0;
}

uint64_t tpw_slp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_slp_layout(int64_t* out) {
    (void)out;
    return 0;
}

// The online system as a host object: native pointers cannot be written by Mac offset.
void tpw_slp_set_system(uint64_t sys) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys)); }
uint64_t tpw_slp_get_system() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineSystem)); }
void tpw_slp_set_list(uint64_t sys, uint32_t v) {
    reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys))->mServerList =
        reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(v));
}
uint32_t tpw_slp_get_list(uint64_t sys) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys))->mServerList));
}

void tpw_slp_a(int32_t error, uint32_t context) { ServerListPostLoginCallbackFn(error, reinterpret_cast<const void*>(static_cast<uintptr_t>(context))); }

}  // extern "C"
