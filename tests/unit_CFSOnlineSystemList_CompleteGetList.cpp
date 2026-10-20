// Test library for CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::CompleteGetList (unit v5662; libtpw_unit_CFSOnlineSystemList_CompleteGetList.dylib,
// tests/test_CFSOnlineSystemList_CompleteGetList.py), main app:
//   src/FS_Online_init_CFSOnlineSystemList_CompleteGetList.cpp   CompleteGetList (0x10174410) and gRideRecordDeleter
//
// The callees outside the unit are recording fakes: AddUIError, the CFSOnlineSystem constructor and Logout, the list's virtual
// StoreReceivedList (a derived test class), the ride deleter and the callback hooks, operator delete and delete[] (only for the pointers
// the test watches; every other pointer goes to free). TbTList::RemoveHead is the real inline template. Globals of other files are
// defined here.
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

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList> RideBase;

// ---- globals of other files
CFSOnlineSystem* gOnlineSystem = nullptr;

template<> RideBase::~CFSOnlineSystemList() {}
ccRideInfo::~ccRideInfo() { ev("InfoDtor", {lab(this)}); }

namespace {

void* gWatchRecv = nullptr;
void* gWatchInfos = nullptr;

struct TestRideList : RideBase {
    void StoreReceivedList(const ccRideList* list) override { ev("Store", {lab(this), lab(list)}); }
};

void CallbackFake(int32_t callback, int32_t error, int32_t context, void* list) {
    ev("Callback", {callback, error, context, lab(list)});
}

void RideDeleteFake(CFSOnlineRide* ride) { ev("RideDelete", {lab(ride), 1}); }

struct Init {
    Init() { gRideRecordDeleter = RideDeleteFake; }
} gInit;

}  // namespace

void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list) = CallbackFake;

// ---- the fakes
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("Ctor", {newlab(this)}); }
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }

// operator delete: the watched received block is recorded and kept, the array of infos too (delete[] hands the block with its cookie).
#if 0
void operator delete(void* p) noexcept {
    if (p == nullptr) return;
    if (p == gWatchRecv) {
        ev("Delete", {lab(p)});
        return;
    }
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p) noexcept {
    if (p == nullptr) return;
    char* c = static_cast<char*>(p);
    char* w = static_cast<char*>(gWatchInfos);
    if (w != nullptr && w >= c && w - c <= 16) {
        ev("DestroyArray", {lab(w), 1});
        return;
    }
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept { ::operator delete[](p); }
#endif

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_cgl_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineSystem));
    return 0;
}

uint64_t tpw_cgl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Host offsets: the list's words 0x04 and 0x28 (mRequestID, mF28), mList, mReceived, then the received block's fields.
int32_t tpw_cgl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TestRideList, mRequestID));
    out[n++] = static_cast<int64_t>(offsetof(TestRideList, mF28));
    out[n++] = static_cast<int64_t>(offsetof(TestRideList, mList));
    out[n++] = static_cast<int64_t>(offsetof(TestRideList, mReceived));
    out[n++] = static_cast<int64_t>(offsetof(ccRideList, mInfos));
    out[n++] = static_cast<int64_t>(offsetof(ccRideList, mF08));
    out[n++] = static_cast<int64_t>(offsetof(ccRideList, mRefCount));
    return n;
}

// Builds the list object: n ride pointers in the list (0 entries are NULL pointers), the received block when hasRecv (with infos made by
// new[] when hasInfos), and watches the received block and the infos. out: list object, mList, received, infos.
void tpw_cgl_build(int32_t n, const uint64_t* rides, int32_t hasRecv, int32_t hasInfos, int32_t refCount, int32_t f08, uint64_t* out) {
    void* block = tpw::ArenaAllocZeroed(sizeof(TestRideList));
    TestRideList* obj = new (block) TestRideList();
    for (int32_t i = 0; i < n; i++) obj->mList->AddTail(reinterpret_cast<CFSOnlineRide*>(static_cast<uintptr_t>(rides[i])));
    ccRideList* recv = nullptr;
    ccRideInfo* infos = nullptr;
    if (hasRecv) {
        recv = new ccRideList();
        recv->mRefCount = refCount;
        recv->mF08 = f08;
        if (hasInfos) {
            infos = new ccRideInfo[2];
            recv->mInfos = infos;
        }
        obj->mReceived = recv;
    }
    gWatchRecv = recv;
    gWatchInfos = infos;
    out[0] = reinterpret_cast<uintptr_t>(obj);
    out[1] = reinterpret_cast<uintptr_t>(obj->mList);
    out[2] = reinterpret_cast<uintptr_t>(recv);
    out[3] = reinterpret_cast<uintptr_t>(infos);
}

// What the call leaves: out = list count, mReceived != NULL, received refcount, received infos != NULL, received mF08.
void tpw_cgl_state(uint64_t object, uint64_t received, int64_t* out) {
    TestRideList* obj = reinterpret_cast<TestRideList*>(static_cast<uintptr_t>(object));
    ccRideList* recv = reinterpret_cast<ccRideList*>(static_cast<uintptr_t>(received));
    out[0] = obj->mList->GetCount();
    out[1] = obj->mReceived != nullptr;
    out[2] = recv ? recv->mRefCount : 0;
    out[3] = recv ? (recv->mInfos != nullptr) : 0;
    out[4] = recv ? recv->mF08 : 0;
}

void tpw_cgl_set_gos(uint64_t value) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(value)); }
uint64_t tpw_cgl_get_gos() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineSystem)); }

void tpw_cgl_complete(uint64_t object, int32_t error) {
    reinterpret_cast<RideBase*>(static_cast<uintptr_t>(object))->CompleteGetList(error);
}

}  // extern "C"
