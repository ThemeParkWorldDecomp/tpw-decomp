// Test library for CFSOnlineSystemParkList_InCity::GetList (unit v5244; libtpw_unit_CFSOnlineSystemParkList_InCity_GetList.dylib), main app:
//   src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList.cpp
//
// The callees outside the unit are fakes that record what the real ones read: the online system's constructor, AreWeLoggedIn and Login,
// PostLoginCallbackFn_Park, the park info destructor, and the global operator new / delete / delete[] (replaced here; they log only
// while a GetList call is running and never free).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

namespace {
bool gTrack = false;
bool gBusy = false;

void Log(const char* name, int64_t a, bool withArg) {
    if (!gTrack || gBusy) return;
    gBusy = true;
    if (withArg)
        ev(name, {a});
    else
        ev(name, {});
    gBusy = false;
}
}  // namespace


ccParkInfo::~ccParkInfo() { Log("park_dtor", 0, false); }

CFSOnlineSystem::CFSOnlineSystem() { Log("ctor", 0, false); }

int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}

void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this), callback == &PostLoginCallbackFn_Park ? 1 : 0, lab(const_cast<void*>(context))});
}

void PostLoginCallbackFn_Park(int32_t error, const void* context) {
    ev("Post", {error, lab(const_cast<void*>(context))});
}

extern "C" {

uint64_t tpw_ilgl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gos", &gOnlineSystem},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ilgl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Blocks the translation frees itself: made by the real operator new / new[] (called before the case's log starts).
uint64_t tpw_ilgl_new(uint32_t size) {
    void* p = ::operator new(size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}
uint64_t tpw_ilgl_new_array(uint32_t size) {
    void* p = ::operator new[](size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

// Host offsets of the Mac's words 0x04..0x28, mReceived (0x30), mF34 (0x34), the object size; the received list's infos, refcount, size.
int32_t tpw_ilgl_layout(int64_t* out) {
    int32_t n = 0;
    char* base = static_cast<char*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemParkList_InCity)));
    auto* o = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(base);
    auto off = [&](const void* member) { return static_cast<int64_t>(static_cast<const char*>(member) - base); };
    out[n++] = off(&o->mRequestID);
    out[n++] = off(&o->mF08);
    out[n++] = off(&o->mF0C);
    out[n++] = off(&o->mF10);
    out[n++] = off(&o->mF14);
    out[n++] = off(&o->mF18);
    out[n++] = off(&o->mF1C);
    out[n++] = off(&o->mF20);
    out[n++] = off(&o->mF24);
    out[n++] = off(&o->mF28);
    out[n++] = off(&o->mReceived);
    out[n++] = off(&o->mF34);
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineSystemParkList_InCity));
    ccParkList* r = nullptr;
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&r->mInfos));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&r->mRefCount));
    out[n++] = static_cast<int64_t>(sizeof(ccParkList));
    return n;
}

void tpw_ilgl_getlist(uint64_t object, int32_t cityID, uint32_t callback, uint32_t context) {
    auto* list = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(static_cast<uintptr_t>(object));
    gTrack = true;
    list->GetList(cityID, reinterpret_cast<CFSOnlineSystemParkList::Callback>(static_cast<uintptr_t>(callback)),
                  reinterpret_cast<const void*>(static_cast<uintptr_t>(context)));
    gTrack = false;
}

}  // extern "C"
