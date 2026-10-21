// Test library for CFSOnlineSystemRideList::RequestListFromServer and StoreReceivedList (unit v5718;
// libtpw_unit_CFSOnlineSystemRideList_RequestListFromServer.dylib), main app:
//   src/game/online/FS_OnlineList.cpp
//
// Callees outside the unit are recording fakes: SamsUtilities::UDebug (text as length and checksum), City::GetRideList (an import,
// answers from the queue), CFSOnlineRide::CFSOnlineRide (logs the new object, the first and last words of the info, the list).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {
void* gList = nullptr;
void* gCity = nullptr;
}  // namespace

// ---- the fakes
void SamsUtilities::UDebug(const char* format, ...) {
    int64_t len = 0, sum = 0;
    for (; format[len]; len++) sum += static_cast<int64_t>(static_cast<unsigned char>(format[len])) * (len + 1);
    ev("UDebug", {len, sum});
}

int32_t City::GetRideList(void* context, uint32_t id, int32_t flag) {
    ev("GetRideList", {lab(this), lab(context), static_cast<int64_t>(id), flag});
    return static_cast<int32_t>(ret("GetRideList"));
}

// The constructor's symbol is defined by a plain function under its mangled name: the real one would build the string members and the
// vtable, which this library does not link.
extern "C" void FakeRideCtor(void* self, const ccRideInfo* info, CFSOnlineSystemRideList* list)
    asm("__ZN13CFSOnlineRideC1EPK10ccRideInfoP23CFSOnlineSystemRideList");
extern "C" void FakeRideCtor(void* self, const ccRideInfo* info, CFSOnlineSystemRideList* list) {
    uint32_t first, last;
    std::memcpy(&first, reinterpret_cast<const unsigned char*>(info), 4);
    std::memcpy(&last, reinterpret_cast<const unsigned char*>(info) + 0x30, 4);
    ev("CFSOnlineRide", {newlab(self), static_cast<int64_t>(first), static_cast<int64_t>(last), lab(list)});
}

extern "C" {

uint64_t tpw_rl_addr(const char* name) {
    if (!gList) gList = tpw::ArenaAllocZeroed(256);
    if (!gCity) gCity = tpw::ArenaAllocZeroed(256);
    if (std::strcmp(name, "list") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gList));
    if (std::strcmp(name, "city") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gCity));
    return 0;
}

uint64_t tpw_rl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ccRideInfo));
    out[n++] = static_cast<int64_t>(offsetof(ccRideList, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ccRideList, mInfos));
    return n;
}

int32_t tpw_rl_request(uint64_t list, uint64_t city, uint32_t id) {
    auto* l = reinterpret_cast<CFSOnlineSystemRideList*>(static_cast<uintptr_t>(list));
    return l->CFSOnlineSystemRideList::RequestListFromServer(reinterpret_cast<City*>(static_cast<uintptr_t>(city)), id);
}

void tpw_rl_store(uint64_t list, uint64_t received) {
    auto* l = reinterpret_cast<CFSOnlineSystemRideList*>(static_cast<uintptr_t>(list));
    l->CFSOnlineSystemRideList::StoreReceivedList(reinterpret_cast<const ccRideList*>(static_cast<uintptr_t>(received)));
}

}  // extern "C"
