// Test library for seven small online system members (unit v5961; libtpw_unit_UploadPostLoginCallbackFn.dylib,
// tests/test_UploadPostLoginCallbackFn.py), main app:
//   src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The globals of other
// files the unit reads (gOnlineSystem, gOnlineConfig, gText) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <malloc/malloc.h>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "TPMain.h"
#include "Text.h"
#include "LbTime.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files
CFSOnlineSystem* gOnlineSystem;
OnlineConfig gOnlineConfig;
OnlineConfig::OnlineConfig() {}
// gText is only referenced (the fake GetLanguageId compares `this` with it): the storage under the symbol's name.
extern "C" {
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");
}

namespace {

uint32_t Fnv(const char* text) {
    uint32_t h = 2166136261u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint8_t>(*text)) * 16777619u;
    return h;
}

int64_t U32(uint32_t v) { return static_cast<int64_t>(v); }
int64_t Ptr(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

}  // namespace

// ---- the runtime: operator new / delete (logged only while a case runs)

namespace v5917_callee {
void Free(void* p) { ev("free", {static_cast<int64_t>(lab(p))}); }
void OperatorDelete(void* p) { ev("delete", {static_cast<int64_t>(lab(p))}); }
}  // namespace v5917_callee

ccCityInfo::~ccCityInfo() { ev("destroy_array", {1}); }   // the one element of the array __destroy_new_array destroys

// ---- the online system's members the unit calls
CFSOnlineSystem::CFSOnlineSystem() { ev("ctor_sys", {static_cast<int64_t>(newlab(this))}); }
void CFSOnlineSystem::ReleaseCityServerConnection() { ev("ReleaseCity", {static_cast<int64_t>(lab(this))}); }
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {static_cast<int64_t>(lab(this))});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
void CFSOnlineSystem::RequestUploadPostLoginCallback(int32_t result, const void* context) {
    ev("RequestUploadPostLoginCallback", {static_cast<int64_t>(lab(this)), U32(static_cast<uint32_t>(result)), Ptr(context)});
}
void CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback(int32_t result, const void* context, const CFSOnlinePark* park) {
    ev("GotoCallback", {static_cast<int64_t>(lab(this)), U32(static_cast<uint32_t>(result)), Ptr(context), static_cast<int64_t>(lab(park))});
}
void CFSOnlineSystem::FTPDownloadCallbackFn(int32_t state, int32_t error) {
    ev("FTPDownloadCallbackFn", {static_cast<int64_t>(lab(this)), U32(static_cast<uint32_t>(state)), U32(static_cast<uint32_t>(error))});
}
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {static_cast<int64_t>(lab(this)), callback == PostLoginCallbackFn_City ? 1 : 0, static_cast<int64_t>(lab(context))});
}
void PostLoginCallbackFn_City(int32_t error, const void* context) {
    ev("PostLoginCallbackFn_City", {U32(static_cast<uint32_t>(error)), static_cast<int64_t>(lab(context))});
}
void FSOnlineCityServerConnectionCallback(int32_t, int32_t, void*, void*) {}

// ---- the online library's City (imports)
City::City() { ev("ctor_city", {static_cast<int64_t>(newlab(this))}); }
int32_t City::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("Connect", {static_cast<int64_t>(lab(this)), U32(Fnv(server)), Ptr(unused), static_cast<int64_t>(port),
                   callback == FSOnlineCityServerConnectionCallback ? 1 : 0});
    return static_cast<int32_t>(ret("Connect"));
}
void City::SetSessionInfo(uint16_t language, int32_t a, int32_t b, char16_t* password) {
    ev("SetSessionInfo", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(language), U32(static_cast<uint32_t>(a)),
                          U32(static_cast<uint32_t>(b)), static_cast<int64_t>(lab(password))});
}
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock", {0});
    return static_cast<uint32_t>(ret("LbTime_GetClock"));
}
uint32_t CText::GetLanguageId() const {
    ev("GetLanguageId", {static_cast<const void*>(this) == static_cast<const void*>(gTextStorage) ? 1 : 0});
    return static_cast<uint32_t>(ret("GetLanguageId"));
}
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {U32(Fnv(format))}); }

extern "C" {

// A zeroed block (32-bit addressable).
uint64_t tpw_u_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// The arena offset a Ptr32 field holds for an address.
uint32_t tpw_u_off(uint64_t p) { return p == 0 ? 0 : static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(p))); }

int32_t tpw_u_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ccThemeInfo));
    out[n++] = static_cast<int64_t>(sizeof(ccRideInfo));
    out[n++] = static_cast<int64_t>(offsetof(ccThemeInfo, mStr04));
    out[n++] = static_cast<int64_t>(offsetof(ccThemeInfo, mStr0C));
    out[n++] = static_cast<int64_t>(offsetof(ccRideInfo, mStr24));
    return n;
}

// The config's city server text and port.
void tpw_u_cfg(const char* text, int32_t port) {
    gOnlineConfig.mCityServer = text;
    gOnlineConfig.mCityPort = static_cast<int16_t>(port);
}
// The size of the block gOnlineSystem points to (the size given to the allocator), 0 when null, -1 when it is not an arena block.
int64_t tpw_u_gsize() {
    if (gOnlineSystem == nullptr) return 0;
    return tpw::InArena(gOnlineSystem) ? 0 : 1;   // made by operator new (not a pool block)
}
void tpw_u_gset(uint64_t p) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(p); }
int64_t tpw_u_gget() { return static_cast<int64_t>(lab(gOnlineSystem)); }

// System fields: 0 mCity (0xd4), 1 mCityClock (0xd8), 2 mCityListsWanted (0xdc), 3 password data (0x28), 4 mF34, 5 mF38.
void tpw_u_sys_set(uint64_t sys, int32_t i, uint64_t v) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(sys);
    switch (i) {
        case 0: s->mCity = reinterpret_cast<City*>(v); break;
        case 1: s->mCityClock = static_cast<uint32_t>(v); break;
        case 2: s->mCityListsWanted = static_cast<int32_t>(v); break;
        case 3: s->mLoginPassword.mData = reinterpret_cast<char16_t*>(v); break;
        case 4: s->mF34 = static_cast<int32_t>(v); break;
        case 5: s->mF38 = static_cast<int32_t>(v); break;
    }
}
int64_t tpw_u_sys_get(uint64_t sys, int32_t i) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(sys);
    switch (i) {
        case 0: return static_cast<int64_t>(lab(s->mCity));
        case 1: return U32(s->mCityClock);
        case 2: return U32(static_cast<uint32_t>(s->mCityListsWanted));
        case 3: return static_cast<int64_t>(lab(s->mLoginPassword.mData));
        case 4: return U32(static_cast<uint32_t>(s->mF34));
        case 5: return U32(static_cast<uint32_t>(s->mF38));
    }
    return 0;
}

// City list fields: 0 mRequestID (4), 1 mF08, 2 mF0C, 3 mF10, 4 mF14, 5 mF18, 6 mF1C, 7 mF20, 8 mF24, 9 mF28, 10 mReceived (0x30).
void tpw_u_list_set(uint64_t list, int32_t i, uint64_t v) {
    CFSOnlineSystemCityList* l = reinterpret_cast<CFSOnlineSystemCityList*>(list);
    int32_t* words[] = {&l->mRequestID, &l->mF08, &l->mF0C, &l->mF10, &l->mF14, &l->mF18, &l->mF1C, &l->mF20, &l->mF24, &l->mF28};
    if (i < 10) *words[i] = static_cast<int32_t>(v);
    else l->mReceived = reinterpret_cast<ccCityList*>(v);
}
int64_t tpw_u_list_get(uint64_t list, int32_t i) {
    CFSOnlineSystemCityList* l = reinterpret_cast<CFSOnlineSystemCityList*>(list);
    int32_t* words[] = {&l->mRequestID, &l->mF08, &l->mF0C, &l->mF10, &l->mF14, &l->mF18, &l->mF1C, &l->mF20, &l->mF24, &l->mF28};
    if (i < 10) return U32(static_cast<uint32_t>(*words[i]));
    return static_cast<int64_t>(lab(l->mReceived));
}
// The stored answer: 0 the infos (a block whose array cookie reads zero), 1 the reference count.
void tpw_u_rcv_set(uint64_t rcv, int32_t i, uint64_t v) {
    ccCityList* r = reinterpret_cast<ccCityList*>(rcv);
    if (i == 0) r->mInfos = v == 0 ? nullptr : new ccCityInfo[1];   // one element: delete[] runs the destructor fake once
    else r->mRefCount = static_cast<int32_t>(v);
}
int64_t tpw_u_rcv_get(uint64_t rcv, int32_t i) {
    ccCityList* r = reinterpret_cast<ccCityList*>(rcv);
    if (i == 0) return r->mInfos != nullptr ? 1 : 0;
    return U32(static_cast<uint32_t>(r->mRefCount));
}
// A list with one node holding `park`.
void tpw_u_mklist(uint64_t list, uint64_t node, uint64_t park) {
    TbPtrList<CFSOnlinePark>* l = reinterpret_cast<TbPtrList<CFSOnlinePark>*>(list);
    TbPtrList<CFSOnlinePark>::CNode* n = reinterpret_cast<TbPtrList<CFSOnlinePark>::CNode*>(node);
    n->mData = reinterpret_cast<CFSOnlinePark*>(park);
    l->mHead = n;
}

// ---- the functions
void tpw_u_upload(int32_t error, uint64_t context) { UploadPostLoginCallbackFn(error, reinterpret_cast<void*>(context)); }
void tpw_u_goto(int32_t error, uint64_t context, uint64_t list) {
    GotoPlayerFTPPostGetParkInfoCallbackFn(error, reinterpret_cast<const void*>(context), reinterpret_cast<TbPtrList<CFSOnlinePark>*>(list));
}
void tpw_u_ftp(uint64_t self, int32_t state, int32_t error) { reinterpret_cast<CFSOnlineFTPControl*>(self)->CallbackFn(state, error); }
int64_t tpw_u_conn(uint64_t sys) {
    return static_cast<int64_t>(lab(reinterpret_cast<CFSOnlineSystem*>(sys)->ReturnConnectionToCityServer()));
}
uint64_t tpw_u_theme(uint64_t p, int32_t flag) {
    ccThemeInfo* t = reinterpret_cast<ccThemeInfo*>(p);
    if (static_cast<int16_t>(flag) == 0 && t != nullptr) t->~ccThemeInfo();
    else ccThemeInfo::Destruct(t, static_cast<int16_t>(flag));
    return p;
}
uint64_t tpw_u_ride(uint64_t p, int32_t flag) {
    ccRideInfo* t = reinterpret_cast<ccRideInfo*>(p);
    if (static_cast<int16_t>(flag) == 0 && t != nullptr) t->~ccRideInfo();
    else ccRideInfo::Destruct(t, static_cast<int16_t>(flag));
    return p;
}
void tpw_u_getlist(uint64_t list, uint64_t callback, uint64_t context) {
    reinterpret_cast<CFSOnlineSystemCityList*>(list)->GetList(
        reinterpret_cast<OnlineListCallback<TbPtrList<CFSOnlineCity>>::Type>(callback), reinterpret_cast<const void*>(context));
}

}  // extern "C"
