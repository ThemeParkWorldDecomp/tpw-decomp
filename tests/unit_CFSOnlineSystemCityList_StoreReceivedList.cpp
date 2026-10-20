// Test library for the city list's three virtual overrides (unit v5307; libtpw_unit_CFSOnlineSystemCityList_StoreReceivedList.dylib,
// tests/test_CFSOnlineSystemCityList_StoreReceivedList.py), main app:
//   src/FS_Online_..._CFSOnlineSystemCityList_StoreReceivedList.cpp   ChildLocked, RequestListFromServer, StoreReceivedList
//
// The callees outside the unit are recording fakes: the CFSOnlineSystem and CFSOnlineCity constructors, GetChildLock, City::GetCityList,
// SamsUtilities::UDebug and operator new. gOnlineSystem (defined by another file) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem;

namespace {
const char* gInfoBase = nullptr;
}

// ---- the fakes
// (operator new is the shim's; its sizes are checked on the PowerPC side by the test module.)
// (the real constructor sets word 0x108; the fake leaves it 0 on both sides: the PowerPC fake's operator new zeroes its block)
CFSOnlineSystem::CFSOnlineSystem() {
    mF108 = 0;
    ev("CFSOnlineSystem", {newlab(this)});
}

uint32_t CFSOnlineSystem::GetChildLock() {
    ev("GetChildLock", {lab(this)});
    return static_cast<uint32_t>(ret("GetChildLock"));
}

CFSOnlineCity::CFSOnlineCity(const ccCityInfo* info, CFSOnlineSystemCityList* list) {
    ev("CFSOnlineCity", {newlab(this), lab(list)});
    ev("CFSOnlineCityInfo", {static_cast<int64_t>(reinterpret_cast<const char*>(info) - gInfoBase)});
}

int32_t City::GetCityList(void* context, uint32_t id, int32_t flag) {
    ev("GetCityList", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(context)), static_cast<int64_t>(id), flag});
    return static_cast<int32_t>(ret("GetCityList"));
}

void SamsUtilities::UDebug(const char* text, ...) { ev("UDebug", {}, {text}); }

extern "C" {

uint64_t tpw_cityl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cityl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cityl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mF108));
    out[n++] = static_cast<int64_t>(offsetof(ccCityList, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ccCityList, mInfos));
    out[n++] = static_cast<int64_t>(sizeof(ccCityList));
    return n;
}

void tpw_cityl_setbase(uint64_t base) { gInfoBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(base)); }

// The label of gOnlineSystem now (the Mac side reads the word and asks the log).
int64_t tpw_cityl_gsys_label() { return lab(gOnlineSystem); }

int32_t tpw_cityl_ChildLocked(uint64_t self) {
    return reinterpret_cast<CFSOnlineSystemCityList*>(static_cast<uintptr_t>(self))->CFSOnlineSystemCityList::ChildLocked();
}

int32_t tpw_cityl_RequestListFromServer(uint64_t self, uint64_t city, uint32_t unused) {
    return reinterpret_cast<CFSOnlineSystemCityList*>(static_cast<uintptr_t>(self))
        ->CFSOnlineSystemCityList::RequestListFromServer(reinterpret_cast<City*>(static_cast<uintptr_t>(city)), unused);
}

void tpw_cityl_StoreReceivedList(uint64_t self, uint64_t list) {
    reinterpret_cast<CFSOnlineSystemCityList*>(static_cast<uintptr_t>(self))
        ->CFSOnlineSystemCityList::StoreReceivedList(reinterpret_cast<const ccCityList*>(static_cast<uintptr_t>(list)));
}

}  // extern "C"
