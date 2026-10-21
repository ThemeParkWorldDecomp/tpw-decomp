// Test library for the online list destructors of the teardown (unit v5L17; libtpw_unit_FS_OnlineList_dtors.dylib,
// tests/test_FS_OnlineList_dtors.py), main app:
//   src/game/online/FS_Online.cpp   the city base, the by-park-id and the top-10 list destructors
//   src/game/online/FS_Online.cpp
//                                 the park list's destructor and the park instance of the base: the host's whole chain runs
//   unit v51065's file (src/FS_Online_..._CFSOnlineSystemCityList_ReturnCityInfo.cpp)   the city list's destructor, for its chain
//
// Fakes: the park's constructor and destructor and the city deleter hook (each logs "recdtor" with the record's index), the park info's
// destructor ("infodtor"), the city info's blocks (the real ccCityInfo::~ frees the block at 0x04, a tag that free() logs as
// "infodtor"), the city list's virtual members, the online system members unit v5921's file calls, the strings.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc/malloc.h>
#include <type_traits>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

namespace {
const void* gRecords[8];
void* gInfoTag = nullptr;

int64_t RecordIndex(const void* record) {
    for (int i = 0; i < 8; i++)
        if (gRecords[i] == record) return i;
    return -1;
}

void DeleteCityFake(CFSOnlineCity* city) {
    ev("recdtor", {RecordIndex(city)});
    delete city;
}

using CityBase = CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>;
}  // namespace

extern "C" void free(void* p) {
    if (p != nullptr && p == gInfoTag) {
        ev("infodtor", {0});
        return;
    }
    if (p != nullptr) {
        malloc_zone_t* zone = malloc_zone_from_ptr(p);
        if (zone != nullptr) malloc_zone_free(zone, p);
    }
}

void (*gCityRecordDeleter)(CFSOnlineCity*) = DeleteCityFake;

// ---- the fakes
#define FAKE_STRING(C)                                                                                             \
    template<> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() {}                                            \
    template<> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}                                           \
    template<> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                           \
    template<> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                              \
    template<> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                           \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }       \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }       \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }      \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
FAKE_STRING(char16_t)
FAKE_STRING(char)

CFSOnlinePark::CFSOnlinePark() {}
CFSOnlinePark::~CFSOnlinePark() { ev("recdtor", {RecordIndex(this)}); }
ccParkInfo::~ccParkInfo() { ev("infodtor", {0}); }

int32_t CFSOnlineSystemCityList::ChildLocked() { return 0; }
int32_t CFSOnlineSystemCityList::RequestListFromServer(City*, uint32_t) { return 0; }
void CFSOnlineSystemCityList::StoreReceivedList(const ccCityList*) {}

void CFSOnlineSystem::Logout() {}
void CFSOnlineSystem::ReleaseNewsConnection() {}
void CFSOnlineSystem::ReleaseUploadServerConnection() {}
void CFSOnlineSystem::ReleaseFTPDownloadServerConnection() {}

// The rest of unit v51065's file (compiled for the city list's destructor): its callees, never reached by a destructor.
CFSOnlineSystem* gOnlineSystem = nullptr;
void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list) = nullptr;
CFSOnlineSystem::CFSOnlineSystem() { std::abort(); }
int32_t CFSOnlineSystem::AreWeLoggedIn() { std::abort(); }
void CFSOnlineSystem::Login(void (*)(int32_t, const void*), const void*) { std::abort(); }
CFSOnlineSystem* CFSOnlineSystem::Instance() { std::abort(); }
City* CFSOnlineSystem::ReturnConnectionToCityServer() { std::abort(); }
void AddUIError(int32_t, TbITable*) { std::abort(); }
ccThemeInfo::~ccThemeInfo() {}
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::LoggedInGetList() { std::abort(); }
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::CompleteGetList(int32_t) { std::abort(); }
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::CompleteGetList(int32_t) { std::abort(); }
template<> CFSOnlineTheme* TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead() { std::abort(); }

// The city list's RemoveHead (0x10177e2c, out of line in unit v51065's file): the real one, the template's body.
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() {
    if (mHead == nullptr) return nullptr;
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    --mCount;
    CFSOnlineCity* value = node->mData;
    delete node;
    return value;
}

namespace {

// Fills a list object: words 0x24 and 0x28, n records (those in nullmask NULL) and the stored answer (recvmode 0 none, 1 count 1 with
// infos, 2 count 1 without, 3 count 2 with infos).
template<class Obj, class Record, class Info>
void Fill(Obj* obj, int32_t n, uint32_t nullmask, int32_t recvmode, int32_t f24, int32_t f28) {
    obj->mF24 = f24;
    obj->mF28 = f28;
    for (int i = 0; i < 8; i++) gRecords[i] = nullptr;
    for (int32_t i = 0; i < n; ++i) {
        Record* record = ((nullmask >> i) & 1) ? nullptr : new Record();
        gRecords[i] = record;
        obj->mList->AddTail(record);
    }
    if (recvmode != 0) {
        auto* received = static_cast<ccReceivedList<Info>*>(::operator new(sizeof(ccReceivedList<Info>)));
        received->mCount = 1;
        received->mInfos = nullptr;
        if (recvmode != 2) {
            received->mInfos = new Info[1];
            std::memset(static_cast<void*>(received->mInfos), 0, sizeof(Info));
        }
        received->mF08 = 0;
        received->mRefCount = recvmode == 3 ? 2 : 1;
        obj->mReceived = received;
    }
}

// Destroys it (flag > 0: the deleting form) and reports the words 0x24 and 0x28 and the count of a kept answer.
template<class Obj>
void Destroy(Obj* obj, int32_t flag, int32_t recvmode, int32_t* out) {
    auto* received = obj->mReceived;
    if (static_cast<int16_t>(flag) > 0) {
        delete obj;
    } else {
        obj->~Obj();
        out[0] = obj->mF24;
        out[1] = obj->mF28;
        if (recvmode == 3) out[2] = received->mRefCount;
        ::operator delete(static_cast<void*>(obj));
    }
}

template<class Obj, class Record, class Info>
void Run(int32_t flag, int32_t n, uint32_t nullmask, int32_t recvmode, int32_t f24, int32_t f28, int32_t* out) {
    Obj* obj = new Obj();
    Fill<Obj, Record, Info>(obj, n, nullmask, recvmode, f24, f28);
    if (recvmode == 1 || recvmode == 3) {
        // The city info's real destructor frees its block at 0x04: a tag that free() logs. The park info's destructor is a fake.
        if constexpr (std::is_same<Info, ccCityInfo>::value) {
            gInfoTag = tpw::ArenaAllocZeroed(8);
            obj->mReceived->mInfos[0].mBlock04 = gInfoTag;
        }
    }
    Destroy(obj, flag, recvmode, out);
    gInfoTag = nullptr;
}

}  // namespace

extern "C" {

int32_t tpw_ld_layout(int64_t* out) {
    (void)out;
    return 0;
}

// kind 0: the city base, 1: the city list, 2: the by-park-id list, 3: the top-10 list. Answers 0 for a NULL object (nothing is run: the
// host never destroys through a NULL pointer), else 1.
uint32_t tpw_ld_dtor(int32_t kind, int32_t flag, int32_t isnull, int32_t n, uint32_t nullmask, int32_t recvmode, int32_t f24, int32_t f28,
                     int32_t* out) {
    out[0] = out[1] = out[2] = 0;
    if (isnull) return 0;
    switch (kind) {
        case 0: Run<CityBase, CFSOnlineCity, ccCityInfo>(flag, n, nullmask, recvmode, f24, f28, out); break;
        case 1: Run<CFSOnlineSystemCityList, CFSOnlineCity, ccCityInfo>(flag, n, nullmask, recvmode, f24, f28, out); break;
        case 2: Run<CFSOnlineSystemParkList_InWorldByParkID, CFSOnlinePark, ccParkInfo>(flag, n, nullmask, recvmode, f24, f28, out); break;
        default: Run<CFSOnlineSystemParkList_InCityTop10, CFSOnlinePark, ccParkInfo>(flag, n, nullmask, recvmode, f24, f28, out); break;
    }
    return 1;
}

}  // extern "C"
