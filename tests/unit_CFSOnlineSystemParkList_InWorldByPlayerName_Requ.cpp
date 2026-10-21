// Test library for CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer (unit v5762;
// libtpw_unit_CFSOnlineSystemParkList_InWorldByPlayerName_Requ.dylib), main app:
//   src/game/online/FS_OnlineList.cpp
//
// The callees outside the unit are fakes recording what the real ones read: SamsUtilities::UDebug and City::GetParkInWorldByName.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

int32_t City::GetParkInWorldByName(void* context, char16_t* name, uint32_t arg, int32_t flag) {
    ev("GetParkInWorldByName", {lab(this), lab(context), static_cast<int32_t>(arg), flag}, {narrow(name).c_str()});
    return static_cast<int32_t>(ret("GetParkInWorldByName"));
}

extern "C" {

uint64_t tpw_parkreq_addr(const char*) { return 0; }

uint64_t tpw_parkreq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_parkreq_layout(int64_t*) { return 0; }

// The object is built here (its text pointer is the string's data), the call is the member's.
uint32_t tpw_parkreq_run(uint64_t city, uint64_t name, uint32_t unused) {
    auto* object = static_cast<CFSOnlineSystemParkList_InWorldByPlayerName*>(
        tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemParkList_InWorldByPlayerName)));
    object->mPlayerName.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(name));
    return static_cast<uint32_t>(
        object->CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer(reinterpret_cast<City*>(static_cast<uintptr_t>(city)), unused));
}

}  // extern "C"
