// Test library for the city list's LoggedInGetList and CompleteGetList (unit v5215; libtpw_unit_CFSOnlineSystemList_LoggedInGetList.dylib,
// tests/test_CFSOnlineSystemList_LoggedInGetList.py), main app:
//   src/game/online/FS_OnlineList.cpp   the two functions, and the two hooks (callback caller, city record deleter)
//
// The callees are recording fakes: AddUIError, CFSOnlineSystem's constructor, Logout, Instance, ReturnConnectionToCityServer, the info
// destructor, the three virtual slots (a test subclass), and the two hooks. gOnlineSystem is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList> CityBase;

// The base destructor is out of line in the program (other files).
template<class L, class R> CFSOnlineSystemList<L, R>::~CFSOnlineSystemList() {}

namespace {

char gCitySentinel[16];
char gInstSentinel[16];
char gSysSentinel[16];

struct TestCityList : CityBase {
    int32_t ChildLocked() override {
        ev("ChildLocked", {lab(this)});
        return static_cast<int32_t>(ret("ChildLocked"));
    }
    int32_t RequestListFromServer(City* city, uint32_t arg) override {
        ev("Request", {lab(this), lab(city), static_cast<int64_t>(arg)});
        return static_cast<int32_t>(ret("Request"));
    }
    void StoreReceivedList(const ccCityList* received) override { ev("Store", {lab(this), lab(received)}); }
};

ccCityList* gRecv;
CFSOnlineCity* gElems[8];

void FakeCallback(int32_t callback, int32_t error, int32_t context, void* list) {
    ev("callback", {callback, error, context, lab(list)});
}
void FakeCityDelete(CFSOnlineCity* city) { ev("city_delete", {lab(city), 1}); }

}  // namespace

// ---- the fakes
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("sys_ctor", {newlab(this)}); }
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    ev("Instance", {});
    return reinterpret_cast<CFSOnlineSystem*>(gInstSentinel);
}
City* CFSOnlineSystem::ReturnConnectionToCityServer() {
    ev("RRC", {lab(this)});
    return ret("RRC") != 0 ? reinterpret_cast<City*>(gCitySentinel) : nullptr;
}
ccCityInfo::~ccCityInfo() { ev("info_dtor", {}); }

extern "C" {

uint64_t tpw_cg_addr(const char* name) {
    if (std::strcmp(name, "city") == 0) return reinterpret_cast<uintptr_t>(gCitySentinel);
    if (std::strcmp(name, "inst") == 0) return reinterpret_cast<uintptr_t>(gInstSentinel);
    if (std::strcmp(name, "sys") == 0) return reinterpret_cast<uintptr_t>(gSysSentinel);
    return 0;
}

uint64_t tpw_cg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cg_layout(int64_t*) { return 0; }

// words: the ten members from mRequestID to mF28; elems: nElems flags (1 a record, 0 a NULL entry); recv: a stored answer.
uint64_t tpw_cg_make(const int32_t* w, const int32_t* elems, int32_t nElems, int32_t hasRecv, int32_t rcount, int32_t rf08,
                     int32_t nInfos, int32_t sysExists) {
    gOnlineListCallbackCaller = FakeCallback;
    gCityRecordDeleter = FakeCityDelete;
    gOnlineSystem = sysExists ? reinterpret_cast<CFSOnlineSystem*>(gSysSentinel) : nullptr;
    TestCityList* o = new TestCityList;
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
    for (int32_t i = 0; i < nElems; i++) {
        gElems[i] = elems[i] ? new CFSOnlineCity : nullptr;
        o->mList->AddTail(gElems[i]);
    }
    gRecv = nullptr;
    if (hasRecv) {
        ccCityInfo* infos = nInfos > 0 ? new ccCityInfo[nInfos] : nullptr;
        gRecv = new ccCityList{nInfos, infos, rf08, rcount};
        o->mReceived = gRecv;
    }
    return reinterpret_cast<uintptr_t>(o);
}

// which: 0 the list, 1 the answer, 2 its infos, 3+i the i-th record.
uint64_t tpw_cg_ptr(uint64_t obj, int32_t which) {
    TestCityList* o = reinterpret_cast<TestCityList*>(obj);
    if (which == 0) return reinterpret_cast<uintptr_t>(o->mList);
    if (which == 1) return reinterpret_cast<uintptr_t>(gRecv);
    if (which == 2) return gRecv != nullptr ? reinterpret_cast<uintptr_t>(gRecv->mInfos) : 0;
    return reinterpret_cast<uintptr_t>(gElems[which - 3]);
}

void tpw_cg_obs(uint64_t obj, int32_t readrc, int32_t* out) {
    TestCityList* o = reinterpret_cast<TestCityList*>(obj);
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
    out[10] = o->mReceived != nullptr;
    out[11] = readrc ? gRecv->mRefCount : 0;
    out[12] = o->mList->GetCount();
    out[13] = gOnlineSystem == nullptr ? 0 : (reinterpret_cast<char*>(gOnlineSystem) == gSysSentinel ? 1 : 2);
}

void tpw_cg_logged(uint64_t obj) { reinterpret_cast<TestCityList*>(obj)->LoggedInGetList(); }
void tpw_cg_complete(uint64_t obj, int32_t error) { reinterpret_cast<TestCityList*>(obj)->CompleteGetList(error); }

}  // extern "C"
