// Test library for CFSOnlineSystemParkList::~CFSOnlineSystemParkList (unit v5154;
// libtpw_unit_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.dylib, tests/test_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.py).
//
// The fakes record what the Mac's callees read: operator delete (global replacement, memory is leaked so a test can read it after),
// the infos' destructor (the Mac's __destroy_new_array call, one info per array), the parks' and the list's deleting destructors.
// The base class's destructor is emptied here: the Mac's derived destructor inlines it, so the translated body does all the work.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"

using namespace calllog;

template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {}

// ---- global replacements: every delete is an event (a leak keeps the memory readable)

ccParkInfo::~ccParkInfo() { ev("destroy_array", {lab(this), 0x10267508}); }

namespace {

// A park: only the virtual destructor matters (slot 1 of the table is the deleting one, like CFSOnlinePark's).
struct FakePark {
    virtual ~FakePark() { ev("park_dtor", {lab(this), 1}); }
    static void operator delete(void*) {}
    int32_t pad[3];
};

struct FakeList : TbPtrList<CFSOnlinePark> {
    ~FakeList() override { ev("list_dtor", {lab(this), 1}); }
    static void operator delete(void*) {}
};

}  // namespace

extern "C" {

uint64_t tpw_pl_new(const int32_t* w) {
    CFSOnlineSystemParkList* o = new CFSOnlineSystemParkList();
    o->mRequestID = w[0];
    o->mF08 = w[1];
    o->mF0C = w[2];
    o->mF10 = w[3];
    o->mF14 = w[4];
    o->mF18 = w[5];
    o->mF1C = w[6];
    o->mF20 = w[7];
    o->mF24 = w[8];
    o->mF28 = w[9];
    o->mList = new FakeList;
    return reinterpret_cast<uintptr_t>(o);
}

void tpw_pl_recv(uint64_t obj, int32_t refcount, int32_t has_infos, int32_t count, int32_t f08, uint64_t* out) {
    CFSOnlineSystemParkList* o = reinterpret_cast<CFSOnlineSystemParkList*>(obj);
    ccParkList* r = new ccParkList;
    r->mCount = count;
    r->mInfos = has_infos ? new ccParkInfo[1] : nullptr;
    r->mF08 = f08;
    r->mRefCount = refcount;
    o->mReceived = r;
    out[0] = reinterpret_cast<uintptr_t>(r);
    out[1] = reinterpret_cast<uintptr_t>(r->mInfos);
}

uint64_t tpw_pl_park(uint64_t obj, int32_t is_null) {
    CFSOnlineSystemParkList* o = reinterpret_cast<CFSOnlineSystemParkList*>(obj);
    CFSOnlinePark* p = is_null ? nullptr : reinterpret_cast<CFSOnlinePark*>(new FakePark);
    o->mList->AddTail(p);
    return reinterpret_cast<uintptr_t>(p);
}

uint64_t tpw_pl_list(uint64_t obj) { return reinterpret_cast<uintptr_t>(reinterpret_cast<CFSOnlineSystemParkList*>(obj)->mList); }

void tpw_pl_run(uint64_t obj, int32_t flag) {
    CFSOnlineSystemParkList* o = reinterpret_cast<CFSOnlineSystemParkList*>(obj);
    if (flag > 0) delete o;
    else o->~CFSOnlineSystemParkList();
}

void tpw_pl_state(uint64_t obj, int64_t* out) {
    CFSOnlineSystemParkList* o = reinterpret_cast<CFSOnlineSystemParkList*>(obj);
    out[0] = o->mRequestID;
    out[1] = o->mF08;
    out[2] = o->mF0C;
    out[3] = o->mF10;
    out[4] = o->mF14;
    out[5] = o->mF18;
    out[6] = o->mF1C;
    out[7] = o->mF20;
    out[8] = o->mF24;
    out[9] = o->mF28;
    out[10] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(o->mReceived));
}

void tpw_pl_recvstate(uint64_t recv, int64_t* out) {
    ccParkList* r = reinterpret_cast<ccParkList*>(recv);
    out[0] = r->mCount;
    out[1] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(r->mInfos));
    out[2] = r->mF08;
    out[3] = r->mRefCount;
}

}  // extern "C"
