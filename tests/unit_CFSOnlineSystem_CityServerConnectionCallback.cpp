// Test library for CFSOnlineSystem::CityServerConnectionCallback (unit v5373; libtpw_unit_CFSOnlineSystem_CityServerConnectionCallback.dylib,
// tests/test_CFSOnlineSystem_CityServerConnectionCallback.py), main app:
//   src/game/online/FS_Online.cpp
//
// The callees outside the unit are fakes that record what the real ones read: UDebug (the text), the info destructors (kind and index
// in the array), operator delete (the tag of the block) and operator delete[] (the array's own block).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

struct Tag {
    const void* address;
    int32_t tag;
};
Tag gTags[64];
int gTagCount = 0;
struct Array {
    const char* base;
    int32_t count;
    int kind;
};
Array gArrays[64];       // the arrays made in this case
int gArrayCount = 0;
const size_t gSize[5] = {sizeof(ccRideInfo), sizeof(ccServerInfo), sizeof(ccThemeInfo), sizeof(ccCityInfo), sizeof(ccParkInfo)};

// The index of an element in the array it belongs to.
int32_t IndexOf(int kind, const void* self) {
    const char* p = static_cast<const char*>(self);
    for (int i = 0; i < gArrayCount; i++) {
        const Array& a = gArrays[i];
        if (a.kind == kind && p >= a.base && p < a.base + gSize[kind] * static_cast<size_t>(a.count))
            return static_cast<int32_t>((p - a.base) / static_cast<std::ptrdiff_t>(gSize[kind]));
    }
    return -1;
}

int32_t TagOf(const void* p) {
    for (int i = 0; i < gTagCount; i++)
        if (gTags[i].address == p) return gTags[i].tag;
    return -1;
}

}  // namespace

namespace SamsUtilities {
void UDebug(const char* text, ...) { ev("UDebug", {}, {text}); }
}  // namespace SamsUtilities

ccRideInfo::~ccRideInfo() { ev("dtor", {0, IndexOf(0, this)}); }
ccServerInfo::~ccServerInfo() { ev("dtor", {1, IndexOf(1, this)}); }
ccThemeInfo::~ccThemeInfo() { ev("dtor", {2, IndexOf(2, this)}); }
ccCityInfo::~ccCityInfo() { ev("dtor", {3, IndexOf(3, this)}); }
ccParkInfo::~ccParkInfo() { ev("dtor", {4, IndexOf(4, this)}); }

// The program's operator delete is replaced in this library: the blocks of the test are never freed.

extern "C" {

// Zeroed blocks made by operator new (the code under test frees some of them with operator delete).
uint64_t tpw_cscb_alloc(uint32_t size) {
    void* p = ::operator new(size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

void tpw_cscb_reset() {
    gTagCount = 0;
    gArrayCount = 0;
}

void tpw_cscb_tag(uint64_t address, int32_t tag) {
    gTags[gTagCount].address = reinterpret_cast<const void*>(static_cast<uintptr_t>(address));
    gTags[gTagCount].tag = tag;
    gTagCount++;
}

int32_t tpw_cscb_tag_of(uint64_t address) { return TagOf(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// An array made with new[] (the cookie delete[] needs), n elements of the kind (0 ride, 1 server, 2 theme, 3 city, 4 park).
uint64_t tpw_cscb_newarray(int32_t kind, int32_t n) {
    void* p = nullptr;
    switch (kind) {
    case 0: p = new ccRideInfo[n]; break;
    case 1: p = new ccServerInfo[n]; break;
    case 2: p = new ccThemeInfo[n]; break;
    case 3: p = new ccCityInfo[n]; break;
    default: p = new ccParkInfo[n]; break;
    }
    gArrays[gArrayCount++] = Array{static_cast<const char*>(p), n, kind};
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

int32_t tpw_cscb_layout(int64_t* out) {
    int32_t n = 0;
    CFSOnlineSystem* sys = nullptr;
    (void)sys;
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mCityList));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksTop10));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksInCity));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksByPlayer));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksByID));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksInCity2));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mThemeList));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mServerList));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mRideList));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemCityList, mRequestID));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemCityList, mF10));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemCityList, mF14));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemCityList, mF18));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemCityList, mReceived));
    out[n++] = static_cast<int64_t>(offsetof(ccReceivedList<ccCityInfo>, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ccReceivedList<ccCityInfo>, mInfos));
    out[n++] = static_cast<int64_t>(offsetof(ccReceivedList<ccCityInfo>, mRefCount));
    return n;
}

void tpw_cscb_call(uint64_t system, int32_t a, int32_t b, uint64_t c, uint64_t d) {
    reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(system))
        ->CityServerConnectionCallback(a, b, reinterpret_cast<void*>(static_cast<uintptr_t>(c)),
                                       reinterpret_cast<void*>(static_cast<uintptr_t>(d)));
}

}  // extern "C"
