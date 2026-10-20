// Test library for CFSOnlineSystemServerList (unit v5713; libtpw_unit_CFSOnlineSystemServerList_RequestListFromServer.dylib), main app:
//   src/CFSOnlineSystemServerList_RequestListFromServer.cpp   GetList, RequestListFromServer, StoreReceivedList
//
// The callees are recording fakes (the PowerPC stubs record the same events). operator new/delete are replaced: new logs one event
// (the host sizes differ from the Mac's), delete logs the pointer's label; new[]/delete[] are silent. gOnlineSystem is defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include <malloc/malloc.h>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;   // 0x10380ab0

namespace {
const char* gInfosBase = nullptr;
}


namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}

int32_t City::GetLogicalServerList(void* context, int32_t a, uint32_t b, int32_t c) {
    ev("GetLogicalServerList", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(context)), a, static_cast<int64_t>(b), c});
    return static_cast<int32_t>(ret("GetLogicalServerList"));
}

CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }

int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}

void ServerListPostLoginCallbackFn(int32_t error, const void* context) {
    ev("ServerListPostLogin", {error, static_cast<int64_t>(reinterpret_cast<uintptr_t>(context))});
}

void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this)});
    ev("LoginCb", {callback == &ServerListPostLoginCallbackFn ? 1 : 0, static_cast<int64_t>(reinterpret_cast<uintptr_t>(context))});
}

// The fake CFSOnlineServer constructor is a plain function under the constructor's symbol: a real constructor would default-construct the
// string members, and the string library is not part of this unit.
extern "C++" void FakeServerCtor(CFSOnlineServer* self, const ccServerInfo* info, CFSOnlineSystemServerList* list)
    __asm__("__ZN15CFSOnlineServerC1EPK12ccServerInfoP25CFSOnlineSystemServerList");
void FakeServerCtor(CFSOnlineServer* self, const ccServerInfo* info, CFSOnlineSystemServerList* list) {
    ev("CFSOnlineServer", {newlab(self), lab(list)});
    ev("serverinfo", {static_cast<int64_t>((reinterpret_cast<const char*>(info) - gInfosBase) / 0x26)});
}

ccServerInfo::~ccServerInfo() {
    ev("dtor", {static_cast<int64_t>((reinterpret_cast<const char*>(this) - gInfosBase) / 0x26)});
}

extern "C" {

uint64_t tpw_sl_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineSystem));
    return 0;
}

uint64_t tpw_sl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A block made with operator new (the original frees the stored answer with operator delete).
uint64_t tpw_sl_allocnew(uint32_t size) {
    void* p = ::operator new(size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

// Nonzero while the block is still allocated (the arena is malloc on the host; the original's delete frees it).

// Whether the block (made by tpw_sl_allocnew(size)) was freed: the platform's operator new hands the freed block out again at once.
uint32_t tpw_sl_wasfreed(uint64_t p, uint32_t size) {
    void* probe = ::operator new(size);
    bool same = reinterpret_cast<uintptr_t>(probe) == static_cast<uintptr_t>(p);
    ::operator delete(probe);
    return same ? 1 : 0;
}

// An array of n infos made with new[] (the cookie holds the count), as the original's mInfos.
uint64_t tpw_sl_newinfos(uint32_t n) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new ccServerInfo[n])); }

void tpw_sl_setbase(uint64_t base) { gInfosBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(base)); }

int32_t tpw_sl_layout(int64_t* out) {
    alignas(16) static char buf[512];
    auto* l = reinterpret_cast<CFSOnlineSystemServerList*>(buf);
    auto* r = reinterpret_cast<ccServerList*>(buf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&l->mRequestID) - buf;
    out[n++] = reinterpret_cast<char*>(&l->mF28) - buf;
    out[n++] = reinterpret_cast<char*>(&l->mList) - buf;
    out[n++] = reinterpret_cast<char*>(&l->mReceived) - buf;
    out[n++] = reinterpret_cast<char*>(&r->mCount) - buf;
    out[n++] = reinterpret_cast<char*>(&r->mInfos) - buf;
    out[n++] = reinterpret_cast<char*>(&r->mF08) - buf;
    out[n++] = reinterpret_cast<char*>(&r->mRefCount) - buf;
    out[n++] = static_cast<int64_t>(sizeof(ccServerList));
    out[n++] = static_cast<int64_t>(sizeof(ccServerInfo));
    return n;
}

static CFSOnlineSystemServerList* L(uint64_t p) { return reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(p)); }

uint32_t tpw_sl_req(uint64_t list, uint64_t city, uint32_t arg) {
    return static_cast<uint32_t>(L(list)->CFSOnlineSystemServerList::RequestListFromServer(reinterpret_cast<City*>(static_cast<uintptr_t>(city)), arg));
}

void tpw_sl_store(uint64_t list, uint64_t recv) {
    L(list)->CFSOnlineSystemServerList::StoreReceivedList(reinterpret_cast<const ccServerList*>(static_cast<uintptr_t>(recv)));
}

void tpw_sl_get(uint64_t list) { L(list)->CFSOnlineSystemServerList::GetList(); }

}  // extern "C"
