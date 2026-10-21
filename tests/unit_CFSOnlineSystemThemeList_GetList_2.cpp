// Test library for the theme list functions (unit v5928; libtpw_unit_CFSOnlineSystemThemeList_GetList_2.dylib,
// tests/test_CFSOnlineSystemThemeList_GetList_2.py), main app:
//   src/game/online/FS_OnlineList.cpp   GetList, RequestListFromServer, StoreReceivedList, ReturnOnlineThemeID, ThemeListPostLoginCallbackFn,
//   and the by-park-id and top-10 park lists' RequestListFromServer.
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The world lives in
// static blocks (the PowerPC side has persistent blocks of its own); gOnlineSystem is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

namespace {
alignas(16) uint8_t gSys[0x1000];
alignas(16) uint8_t gTl[0x200];
alignas(16) uint8_t gCity[0x100];
uint8_t* gRcvP;     // the stored answer and its infos array (cookie in the first 8 bytes, the infos from +8) come from the arena:
uint8_t* gInfosP;   // the list frees them with operator delete / delete[]
alignas(16) uint8_t gPark[0x100];
alignas(16) uint8_t gPool[0x800];
}  // namespace

// ---- the fakes

// The array of infos is destroyed last to first by delete[]; the destructor is the only visible part of it (operator new and delete are the
// program's own here). The module's PowerPC stub of the array destroyer logs the same events.
ccThemeInfo::~ccThemeInfo() {
    int64_t index = (reinterpret_cast<uint8_t*>(this) - gInfosP) / 16;
    if (index == 31) ev("destroy_array", {lab(gInfosP)});
    ev("~ccThemeInfo", {index});
}

// The theme's strings are never made (the theme constructor is a fake), but the constructor and vtable name them.
typedef TbDynamicStringTemplate<char> DynStr;
template<> DynStr::TbDynamicStringTemplate() {}
template<> DynStr::~TbDynamicStringTemplate() {}
template<> void DynStr::Format(const char*, ...) {}
template<> void DynStr::ResizeTo(uint32_t) {}
template<> void DynStr::AllocBuffer(uint32_t) {}
template<> DynStr& DynStr::operator=(const TbStringBase<char>&) { return *this; }
template<> DynStr& DynStr::operator=(const DynStr&) { return *this; }
template<> DynStr& DynStr::operator=(const char*) { return *this; }
template<> DynStr& DynStr::operator=(const char&) { return *this; }
template<> DynStr& DynStr::operator+=(const TbStringBase<char>&) { return *this; }
template<> DynStr& DynStr::operator+=(const char&) { return *this; }
template<> DynStr& DynStr::operator+=(const char*) { return *this; }

CFSOnlineSystem::CFSOnlineSystem() {
    std::memset(static_cast<void*>(this), 0, sizeof(CFSOnlineSystem));   // the PowerPC side's new memory is zero
    ev("CFSOnlineSystem_ct", {}); }
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this), callback == &ThemeListPostLoginCallbackFn ? 1 : 0, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)))});
}
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::CompleteGetList(int32_t error) {
    ev("CompleteGetList", {lab(this), error});
}
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::LoggedInGetList() { ev("LoggedInGetList", {lab(this)}); }
CFSOnlineTheme::CFSOnlineTheme(const ccThemeInfo* info, CFSOnlineSystemThemeList* list) {
    ev("CFSOnlineTheme_ct", {lab(list), static_cast<int64_t>(info->mF00)});
}
CFSOnlineTheme::~CFSOnlineTheme() {}

int32_t City::GetThemeList(void* context, uint32_t arg, int32_t flag) {
    ev("GetThemeList", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context))), arg, flag});
    return static_cast<int32_t>(ret("GetThemeList"));
}
int32_t City::GetParkByID(void* context, int32_t park, uint32_t arg, int32_t flag) {
    ev("GetParkByID", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context))), park, arg, flag});
    return static_cast<int32_t>(ret("GetParkByID"));
}
int32_t City::GetParkInCityListTop10(void* context, int32_t city, uint32_t arg, int32_t flag) {
    ev("GetParkInCityListTop10", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context))), city, arg, flag});
    return static_cast<int32_t>(ret("GetParkInCityListTop10"));
}

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    int32_t d = strcasecmp(a, b);
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

extern "C" {

uint64_t tpw_tl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem}, {"sys", gSys},   {"tl", gTl},     {"city", gCity}, {"rcv", gRcvP},
        {"infos", gInfosP},          {"park", gPark}, {"pool", gPool},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_tl_fresh() {
    gRcvP = static_cast<uint8_t*>(::operator new(0x40));
    gInfosP = reinterpret_cast<uint8_t*>(new ccThemeInfo[0x20]);   // the list releases it with delete[]
    std::memset(gRcvP, 0, 0x40);
    std::memset(gInfosP, 0, 0x200);
}

// The host offsets of the fields the module writes (HNAMES in the module, same order).
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int32_t tpw_tl_layout(int64_t* out) {
    int32_t n = 0;
    typedef TbTList<CFSOnlineTheme*, CFSOnlineTheme*> TList;
    out[n++] = offsetof(CFSOnlineSystemThemeList, mRequestID);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF08);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF0C);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF10);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF14);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF18);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF1C);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mF20);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mList);
    out[n++] = offsetof(CFSOnlineSystemThemeList, mReceived);
    out[n++] = offsetof(CFSOnlineSystemParkList_InWorldByParkID, mF34);
    out[n++] = offsetof(CFSOnlineSystemParkList_InCityTop10, mF34);
    out[n++] = offsetof(CFSOnlineSystem, mThemeList);
    out[n++] = offsetof(ccThemeList, mCount);
    out[n++] = offsetof(ccThemeList, mInfos);
    out[n++] = offsetof(ccThemeList, mF08);
    out[n++] = offsetof(ccThemeList, mRefCount);
    out[n++] = sizeof(ccThemeInfo);
    out[n++] = offsetof(TList, mHead);
    out[n++] = offsetof(TList::CNode, mData);
    out[n++] = offsetof(TList::CNode, mNext);
    out[n++] = offsetof(CFSOnlineTheme, mThemeID);
    out[n++] = offsetof(CFSOnlineTheme, mText0C) + offsetof(TbDynamicStringTemplate<char>, mData);
    return n;
}

static CFSOnlineSystemThemeList* TL(uint64_t p) { return reinterpret_cast<CFSOnlineSystemThemeList*>(p); }
static City* CT(uint64_t p) { return reinterpret_cast<City*>(p); }

void tpw_tl_getlist(uint64_t list) { TL(list)->GetList(); }
int32_t tpw_tl_request(uint64_t list, uint64_t city, uint32_t arg) { return TL(list)->CFSOnlineSystemThemeList::RequestListFromServer(CT(city), arg); }
void tpw_tl_store(uint64_t list, uint64_t rcv) { TL(list)->CFSOnlineSystemThemeList::StoreReceivedList(reinterpret_cast<const ccThemeList*>(rcv)); }
uint32_t tpw_tl_retid(uint64_t list, uint64_t name) { return TL(list)->ReturnOnlineThemeID(reinterpret_cast<const char*>(name)); }
void tpw_tl_cb(int32_t error) { ThemeListPostLoginCallbackFn(error, nullptr); }
int32_t tpw_tl_pbid(uint64_t obj, uint64_t city, uint32_t arg) {
    return reinterpret_cast<CFSOnlineSystemParkList_InWorldByParkID*>(obj)->CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(CT(city), arg);
}
int32_t tpw_tl_top10(uint64_t obj, uint64_t city, uint32_t arg) {
    return reinterpret_cast<CFSOnlineSystemParkList_InCityTop10*>(obj)->CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(CT(city), arg);
}

}  // extern "C"
