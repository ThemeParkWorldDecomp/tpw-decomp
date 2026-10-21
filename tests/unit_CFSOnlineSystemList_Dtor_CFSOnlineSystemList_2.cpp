// Test library for three destructors of the online system (unit v5921; libtpw_unit_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.dylib,
// tests/test_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.py), main app:
//   src/game/online/FS_Online.cpp   the park list's, ccCityInfo's and CFSOnlineSystem's destructors
//
// The callees outside the unit are fakes that record the same events as the module's PowerPC stubs. Objects the destructors delete through
// a virtual call (lists, the FTP park) are "Fake" objects whose deleting destructor logs "vdel"; every operator delete is logged, and
// memory from operator new carries a header so that arena memory is never handed to the C library. free() is interposed to log.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc/malloc.h>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

namespace {

const uint64_t kMagic = 0x7470774d41474943ull;
const void* gStr[12];                       // the system object's string members (host addresses) ...
const int gStrMac[12] = {0xc, 0x20, 0x3c, 0x7c, 0x98, 0xb4, 0x10c, 0x330, 0x348, 0x35c, 0x3ac, 0x3dc};   // ... and the Mac offsets
void* gObj[16];
bool gFreeLog = false;

// A label for an address: the label of a known object, 1000 + the offset inside the system object, or -1.
int64_t where(const void* p) {
    const char* c = static_cast<const char*>(p);
    for (int i = 0; i < 12; i++)
        if (gStr[i] == p) return 1000 + gStrMac[i];
    return lab(const_cast<void*>(p));
}

void* Raw(std::size_t n) {
    void* p = ::operator new(n);
    std::memset(p, 0, n);
    return p;
}

struct Fake {
    virtual ~Fake() { ev("vdel", {where(this), 1}); }
    uint64_t mPad = 0;
};

using ParkListInst = CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>;

}  // namespace


extern "C" void free(void* p) {
    if (gFreeLog) {
        ev("free", {lab(p)});
        return;
    }
    if (p != nullptr) {
        malloc_zone_t* zone = malloc_zone_from_ptr(p);
        if (zone != nullptr) malloc_zone_free(zone, p);
    }
}

// ---- the fakes
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("str_dtor_c", {where(this)}); }
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { ev("str_dtor_w", {where(this)}); }
// The vtables of the strings need every virtual defined (the dtor is the only one the unit runs).
#define TPW_STR_VIRTUALS(CH)                                                                                                    \
    template<> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                                        \
    template<> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                                       \
    template<> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                           \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; }    \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; }  \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; }                 \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }                 \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }                \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }
TPW_STR_VIRTUALS(char16_t)
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

ccParkInfo::~ccParkInfo() { ev("destroy_array", {where(this)}); }

void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
void CFSOnlineSystem::ReleaseNewsConnection() { ev("ReleaseNews", {lab(this)}); }
void CFSOnlineSystem::ReleaseUploadServerConnection() { ev("ReleaseUpload", {lab(this)}); }
void CFSOnlineSystem::ReleaseFTPDownloadServerConnection() { ev("ReleaseFTP", {lab(this)}); }

extern "C" {

uint64_t tpw_dtors_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_dtors_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_dtors_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ccCityInfo, mBlock04));
    out[n++] = static_cast<int64_t>(offsetof(ccCityInfo, mBlock08));
    out[n++] = static_cast<int64_t>(offsetof(ccCityInfo, mBlock1C));
    return n;
}

// A fake object that logs "vdel" when deleted through a virtual call; registered as label `label` for RemoveHead.
uint64_t tpw_dtors_mkobj(int32_t label) {
    Fake* f = new Fake;
    gObj[label] = f;
    return reinterpret_cast<uintptr_t>(f);
}

// ---- ccCityInfo::~ccCityInfo
void tpw_dtors_city(uint64_t object, int32_t flag) {
    ccCityInfo* o = reinterpret_cast<ccCityInfo*>(object);
    gFreeLog = true;
    o->~ccCityInfo();
    gFreeLog = false;
    if (static_cast<int16_t>(flag) > 0) ::operator delete(static_cast<void*>(o));
}

// ---- the park list
uint64_t tpw_dtors_t1_build(int32_t count, int32_t rcv, int32_t refcount, int32_t info, int32_t f24, int32_t f28, const uint64_t* parks, uint64_t* out) {
    ParkListInst* o = new ParkListInst();
    o->mF24 = f24;
    o->mF28 = f28;
    for (int i = 0; i < count; i++) o->mList->AddTail(reinterpret_cast<CFSOnlinePark*>(parks[i]));
    {
        auto* node = o->mList->mHead;
        for (int i = 0; i < count && node != nullptr; i++, node = node->mNext) out[4 + i] = reinterpret_cast<uintptr_t>(node);
    }
    ccParkList* r = nullptr;
    ccParkInfo* infos = nullptr;
    if (rcv) {
        r = static_cast<ccParkList*>(Raw(sizeof(ccParkList)));
        r->mRefCount = refcount;
        if (info) {
            infos = new ccParkInfo[1];
            r->mInfos = infos;
        }
        o->mReceived = r;
    }
    out[0] = reinterpret_cast<uintptr_t>(o);
    out[1] = reinterpret_cast<uintptr_t>(o->mList);
    out[2] = reinterpret_cast<uintptr_t>(r);
    out[3] = reinterpret_cast<uintptr_t>(infos);
    return out[0];
}

void tpw_dtors_t1_run(uint64_t object, int32_t flag, int64_t* snap) {
    ParkListInst* o = reinterpret_cast<ParkListInst*>(object);
    ccParkList* r = o->mReceived;
    bool alive = r != nullptr && static_cast<int32_t>(r->mRefCount - 1) != 0;
    o->~ParkListInst();
    snap[0] = static_cast<uint32_t>(o->mF24);
    snap[1] = static_cast<uint32_t>(o->mF28);
    snap[2] = o->mReceived != nullptr;
    snap[3] = alive ? static_cast<uint32_t>(r->mRefCount) : 0;
    if (static_cast<int16_t>(flag) > 0) ::operator delete(static_cast<void*>(o));
}

// ---- CFSOnlineSystem: nine lists (0..8) and the FTP park (9) as fake objects where the mask has the bit
uint64_t tpw_dtors_t3_build(uint32_t mask, uint64_t* out) {
    CFSOnlineSystem* s = static_cast<CFSOnlineSystem*>(Raw(sizeof(CFSOnlineSystem)));
    for (int i = 0; i < 10; i++) {
        out[i] = 0;
        if (!(mask & (1u << i))) continue;
        Fake* f = new Fake;
        out[i] = reinterpret_cast<uintptr_t>(f);
        switch (i) {
            case 0: s->mCityList = reinterpret_cast<CFSOnlineSystemCityList*>(f); break;
            case 1: s->mParksTop10 = reinterpret_cast<CFSOnlineSystemParkList_InCityTop10*>(f); break;
            case 2: s->mParksInCity = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(f); break;
            case 3: s->mParksByPlayer = reinterpret_cast<CFSOnlineSystemParkList_InWorldByPlayerName*>(f); break;
            case 4: s->mParksByID = reinterpret_cast<CFSOnlineSystemParkList_InWorldByParkID*>(f); break;
            case 5: s->mParksInCity2 = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(f); break;
            case 6: s->mThemeList = reinterpret_cast<CFSOnlineSystemThemeList*>(f); break;
            case 7: s->mServerList = reinterpret_cast<CFSOnlineSystemServerList*>(f); break;
            case 8: s->mRideList = reinterpret_cast<CFSOnlineSystemRideList*>(f); break;
            default: s->mFTPPark = reinterpret_cast<CFSOnlinePark*>(f); break;
        }
    }
    gStr[0] = &s->mLoginName;
    gStr[1] = &s->mLoginPassword;
    gStr[2] = &s->mLoginEmail;
    gStr[3] = &s->mNewsText;
    gStr[4] = &s->mF98;
    gStr[5] = &s->mFB4;
    gStr[6] = &s->mF10C;
    gStr[7] = &s->mF330;
    gStr[8] = &s->mF348;
    gStr[9] = &s->mF35C;
    gStr[10] = &s->mF3AC;
    gStr[11] = &s->mF3DC;
    return reinterpret_cast<uintptr_t>(s);
}

void tpw_dtors_t3_run(uint64_t object, int32_t flag) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(object);
    s->~CFSOnlineSystem();
    if (static_cast<int16_t>(flag) > 0) ::operator delete(static_cast<void*>(s));
    for (int i = 0; i < 12; i++) gStr[i] = nullptr;
}

}  // extern "C"
