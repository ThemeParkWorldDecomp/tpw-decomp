// Test library for the online lists' small members, the park lists' GetList (top ten, by park id), the login callbacks, the city list's
// and the theme list base's destructors, the park instance of LoggedInGetList and the FTP getters (unit v51065;
// libtpw_unit_CFSOnlineSystemCityList_ReturnCityInfo.dylib, tests/test_CFSOnlineSystemCityList_ReturnCityInfo.py), main app:
//   src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp, src/game/save/FS_MasterSave.cpp
//
// The callees outside the unit are fakes that record what the real ones read (the online system, the city server connection, the list
// virtuals, the info destructors, RemoveHead, AddUIError, the callback caller, operator new and delete while a function runs). The globals
// of other files (gOnlineSystem and the two hooks) are defined here, zero or default at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>

#include "platform/arena.h"

// operator delete keeps the arena's blocks (the world's lists and records are arena memory) and every block freed while a function
// runs (the Mac's delete is a stub there, and the observation reads the list and the answer after a deleting destructor).
namespace {
bool KeepFreed(void* p);
}
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p)
#include "calllog.cpp"

#define TPW_V5928_STRONG 1
#include "FS_Online.h"
#include "FS_OnlineList.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {
bool gInCall = false;     // operator new and delete record only while a function under test runs
bool gBusy = false;       // inside the recorder itself (its own allocations are not events)

struct Scope {
    Scope() { gInCall = true; }
    ~Scope() { gInCall = false; }
};

void Ev(const char* name, std::initializer_list<int64_t> ints) {
    bool was = gBusy;
    gBusy = true;
    ev(name, ints);
    gBusy = was;
}

void* gCityToken = nullptr;       // what the city server connection fake answers when its queue says yes
void* gRecords[16] = {};          // what RemoveHead answers: the queue holds an index into this table (0 is NULL)

// The labels of the case's objects (set by the test through tpw_crl_label): 0 for NULL, -1 for an address nobody named.
std::map<uint64_t, int64_t> gIds;
int64_t L(const void* p) {
    if (p == nullptr) return 0;
    auto it = gIds.find(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)));
    return it == gIds.end() ? -1 : it->second;
}
}  // namespace

// ---- operator delete (calllog.cpp's, with the observer above)
namespace {
bool KeepFreed(void* p) { return tpw::InArena(p) || gInCall; }
}  // namespace

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem;
namespace {
void HostCallbackCaller(int32_t callback, int32_t error, int32_t context, void* list) {
    Ev("Callback", {static_cast<int64_t>(static_cast<uint32_t>(callback)), error, static_cast<int64_t>(static_cast<uint32_t>(context)), L(list)});
}
void HostCityDeleter(CFSOnlineCity* city) {
    Ev("RecordDtor", {L(city)});
}
}  // namespace
void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list) = HostCallbackCaller;
void (*gCityRecordDeleter)(CFSOnlineCity* city) = HostCityDeleter;

// ---- the online system and the free functions
CFSOnlineSystem::CFSOnlineSystem() {
    Ev("CFSOnlineSystem_ctor", {});
    gIds[static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this))] = 99;
}
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    Ev("AreWeLoggedIn", {L(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    Ev("Login", {L(this), callback == &PostLoginCallbackFn_Park ? 1 : 0, L(context)});
}
void CFSOnlineSystem::Logout() { Ev("Logout", {L(this)}); }
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    Ev("Instance", {});
    return gOnlineSystem;
}
City* CFSOnlineSystem::ReturnConnectionToCityServer() {
    Ev("ReturnConnection", {L(this)});
    return ret("ReturnConnection") != 0 ? static_cast<City*>(gCityToken) : nullptr;
}
void AddUIError(int32_t error, TbITable* table) { Ev("AddUIError", {error, L(table)}); }

// ---- the list instances outside the unit
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::LoggedInGetList() { Ev("LoggedInGetList_City", {L(this)}); }
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::CompleteGetList(int32_t error) {
    Ev("CompleteGetList_City", {L(this), error});
}
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::CompleteGetList(int32_t error) {
    Ev("CompleteGetList", {L(this), error});
}
// The empty base destructors of the instances the unit does not translate (the unit's derived destructors do the base's work),
// as the specializations they are in the program (FS_Online.cpp), which the port's build of the header declares.
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {}

CFSOnlineSystemParkList::~CFSOnlineSystemParkList() {}
CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity() {}
CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10() {}
CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID() {}
int32_t CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(City* city, uint32_t arg) {
    Ev("Request", {L(this), L(city), static_cast<int64_t>(arg)});
    return static_cast<int32_t>(ret("Request"));
}
int32_t CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(City* city, uint32_t arg) {
    Ev("Request", {L(this), L(city), static_cast<int64_t>(arg)});
    return static_cast<int32_t>(ret("Request"));
}
CFSOnlineSystemThemeList::CFSOnlineSystemThemeList() {}
CFSOnlineSystemThemeList::~CFSOnlineSystemThemeList() {}
// Unit v51129's empty C++ destructor of the theme instance (src/game/online/FS_OnlineList.cpp, not in this library).
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::~CFSOnlineSystemList() {}
int32_t CFSOnlineSystemThemeList::RequestListFromServer(City*, uint32_t) { return 0; }
void CFSOnlineSystemThemeList::StoreReceivedList(const ccThemeList*) {}
int32_t CFSOnlineSystemCityList::ChildLocked() { return 0; }
int32_t CFSOnlineSystemCityList::RequestListFromServer(City*, uint32_t) { return 0; }
void CFSOnlineSystemCityList::StoreReceivedList(const ccCityList*) {}

namespace {
// A park list whose virtuals record, for LoggedInGetList and the login callback.
class TestParkList : public CFSOnlineSystemParkList {
public:
    int32_t ChildLocked() override {
        Ev("ChildLocked", {L(this)});
        return static_cast<int32_t>(ret("ChildLocked"));
    }
    int32_t RequestListFromServer(City* city, uint32_t arg) override {
        Ev("Request", {L(this), L(city), static_cast<int64_t>(arg)});
        return static_cast<int32_t>(ret("Request"));
    }
};
}  // namespace

// ---- the infos' destructors (the events are in the order of the elements, which are not told apart)
ccCityInfo::~ccCityInfo() { Ev("~info", {1}); }
ccParkInfo::~ccParkInfo() { Ev("~info", {2}); }
ccThemeInfo::~ccThemeInfo() { Ev("~info", {3}); }

// ---- RemoveHead of the city and theme lists: one node fewer, the record the queue names (an index into gRecords)
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() {
    Ev("RemoveHead", {L(this)});
    if (mCount > 0) --mCount;
    return static_cast<CFSOnlineCity*>(gRecords[ret("RemoveHead") & 15]);
}
template<> CFSOnlineTheme* TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead() {
    Ev("RemoveHead", {L(this)});
    if (mCount > 0) --mCount;
    return static_cast<CFSOnlineTheme*>(gRecords[ret("RemoveHead") & 15]);
}

namespace {
// A theme record's virtual destructor without the class's strings: a hand-made Itanium vtable (offset, type, D1, D0).
void FakeThemeD1(CFSOnlineTheme* self) { Ev("RecordDtor", {L(self)}); }
void FakeThemeD0(CFSOnlineTheme* self) {
    Ev("RecordDtor", {L(self)});
    ::operator delete(static_cast<void*>(self));
}
void* gFakeThemeVtable[4] = {nullptr, nullptr, reinterpret_cast<void*>(&FakeThemeD1), reinterpret_cast<void*>(&FakeThemeD0)};

template<class T> T* Arena() { return static_cast<T*>(tpw::ArenaAllocZeroed(sizeof(T))); }

typedef CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList> ParkBase;
typedef TbTList<CFSOnlineCity*, CFSOnlineCity*> CityNodes;
typedef TbTList<CFSOnlinePark*, CFSOnlinePark*> ParkNodes;
typedef TbTList<CFSOnlineTheme*, CFSOnlineTheme*> ThemeNodes;

template<class P> P* Ptr(uint64_t value) { return reinterpret_cast<P*>(static_cast<uintptr_t>(value)); }
uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

extern "C" {

uint64_t tpw_crl_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return U(&gOnlineSystem);
    return 0;
}

void tpw_crl_label(uint64_t address, int64_t id) { gIds[address] = id; }
void tpw_crl_reset() {
    gIds.clear();
    gCityToken = nullptr;
    for (auto& r : gRecords) r = nullptr;
}

uint64_t tpw_crl_alloc(uint32_t size) { return U(tpw::ArenaAllocZeroed(size)); }

// mList, mReceived and the derived classes' word at 0x34 (HOST_LAYOUT).
int32_t tpw_crl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ParkBase, mList));
    out[n++] = static_cast<int64_t>(offsetof(ParkBase, mReceived));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystemParkList_InCityTop10, mF34));
    out[n++] = static_cast<int64_t>(offsetof(ParkBase, mRequestID));
    return n;
}

// ---- the world
// kind: 0 city list, 1 theme list base, 2 top ten, 3 by park id, 4 recording park list, 5 in-city park list, 6 theme list.
uint64_t tpw_crl_new_list(uint32_t kind) {
    void* memory = tpw::ArenaAllocZeroed(256);
    switch (kind) {
        case 0: return U(new (memory) CFSOnlineSystemCityList());
        case 1: return U(new (memory) CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>());
        case 2: return U(new (memory) CFSOnlineSystemParkList_InCityTop10());
        case 3: return U(new (memory) CFSOnlineSystemParkList_InWorldByParkID());
        case 4: return U(new (memory) TestParkList());
        case 5: return U(new (memory) CFSOnlineSystemParkList_InCity());
        default: return U(new (memory) CFSOnlineSystemThemeList());
    }
}

// The words of the list by Mac offset: 0x04..0x28 the ints, 0x2c the list, 0x30 the stored answer, 0x34 the derived word.
uint64_t tpw_crl_getw(uint64_t object, uint64_t off) {
    ParkBase* list = Ptr<ParkBase>(object);
    if (off >= 4 && off <= 0x28) return static_cast<uint32_t>((&list->mRequestID)[(off - 4) / 4]);
    if (off == 0x2c) return U(list->mList);
    if (off == 0x30) return U(list->mReceived);
    return static_cast<uint32_t>(Ptr<CFSOnlineSystemParkList_InCityTop10>(object)->mF34);
}
void tpw_crl_setw(uint64_t object, uint64_t off, uint64_t value) {
    ParkBase* list = Ptr<ParkBase>(object);
    if (off >= 4 && off <= 0x28) {
        (&list->mRequestID)[(off - 4) / 4] = static_cast<int32_t>(value);
    } else if (off == 0x2c) {
        list->mList = Ptr<TbPtrList<CFSOnlinePark>>(value);
    } else if (off == 0x30) {
        list->mReceived = Ptr<ccParkList>(value);
    } else {
        Ptr<CFSOnlineSystemParkList_InCityTop10>(object)->mF34 = static_cast<int32_t>(value);
    }
}

// The typed list: count (Mac 0x0c) and AddTail. rk: 0 city, 1 park, 2 theme records.
void tpw_crl_set_count(uint64_t nodes, uint64_t count) { Ptr<CityNodes>(nodes)->mCount = static_cast<int32_t>(count); }
void tpw_crl_add_tail(uint64_t rk, uint64_t nodes, uint64_t record) {
    if (rk == 0) Ptr<CityNodes>(nodes)->AddTail(Ptr<CFSOnlineCity>(record));
    else if (rk == 1) Ptr<ParkNodes>(nodes)->AddTail(Ptr<CFSOnlinePark>(record));
    else Ptr<ThemeNodes>(nodes)->AddTail(Ptr<CFSOnlineTheme>(record));
}
uint64_t tpw_crl_new_rec(uint64_t rk, uint64_t id, uint64_t name) {
    if (rk == 0) {
        CFSOnlineCity* city = Arena<CFSOnlineCity>();
        city->mCityID = static_cast<int32_t>(id);
        return U(city);
    }
    if (rk == 1) {
        CFSOnlinePark* park = Arena<CFSOnlinePark>();
        park->mParkID = static_cast<int32_t>(id);
        return U(park);
    }
    CFSOnlineTheme* theme = Arena<CFSOnlineTheme>();
    *reinterpret_cast<void***>(theme) = &gFakeThemeVtable[2];
    theme->mThemeID = static_cast<int32_t>(id);
    theme->mText0C.mData = Ptr<char>(name);
    return U(theme);
}
// The stored answer: rk 0 city, 1 park, 2 theme. hasInfos: the infos made with new[] (n elements), else NULL.
uint64_t tpw_crl_new_recv(uint64_t rk, uint64_t n, uint64_t ref, uint64_t hasInfos) {
    int32_t count = static_cast<int32_t>(n);
    if (rk == 0) {
        ccCityList* r = new ccCityList();
        r->mCount = count;
        r->mInfos = hasInfos ? new ccCityInfo[n] : nullptr;
        r->mRefCount = static_cast<int32_t>(ref);
        return U(r);
    }
    if (rk == 1) {
        ccParkList* r = new ccParkList();
        r->mCount = count;
        r->mInfos = hasInfos ? new ccParkInfo[n] : nullptr;
        r->mRefCount = static_cast<int32_t>(ref);
        return U(r);
    }
    ccThemeList* r = new ccThemeList();
    r->mCount = count;
    r->mInfos = hasInfos ? new ccThemeInfo[n] : nullptr;
    r->mRefCount = static_cast<int32_t>(ref);
    return U(r);
}
uint64_t tpw_crl_recv_ref(uint64_t received) { return static_cast<uint32_t>(Ptr<ccParkList>(received)->mRefCount); }
uint64_t tpw_crl_new_sys() { return U(Arena<CFSOnlineSystem>()); }
void tpw_crl_sys_set(uint64_t sys, uint64_t park, uint64_t data) {
    CFSOnlineSystem* system = Ptr<CFSOnlineSystem>(sys);
    system->mFTPPark = Ptr<CFSOnlinePark>(park);
    system->mF3DC.mData = Ptr<char>(data);
}
void tpw_crl_set_sys_global(uint64_t sys) { gOnlineSystem = Ptr<CFSOnlineSystem>(sys); }
void tpw_crl_set_city(uint64_t token) { gCityToken = Ptr<void>(token); }
void tpw_crl_set_rec(uint64_t index, uint64_t record) { gRecords[index & 15] = Ptr<void>(record); }

// ---- the functions (every wrapper takes four words and answers one; the events of operator new and delete are recorded inside the call)
uint64_t tpw_crl_f_city_info(uint64_t list, uint64_t id, uint64_t, uint64_t) {
    Scope scope;
    return U(park_info_callee::ReturnCityInfo(Ptr<CFSOnlineSystemCityList>(list), static_cast<int32_t>(id)));
}
uint64_t tpw_crl_f_park_info(uint64_t list, uint64_t index, uint64_t, uint64_t) {
    Scope scope;
    return U(Ptr<CFSOnlineSystemParkList_InCity>(list)->ReturnParkInfo(static_cast<int32_t>(index)));
}
uint64_t tpw_crl_f_theme_name(uint64_t list, uint64_t id, uint64_t, uint64_t) {
    Scope scope;
    return U(Ptr<CFSOnlineSystemThemeList>(list)->ReturnOnlineThemeName(static_cast<int32_t>(id)));
}
uint64_t tpw_crl_f_ftp_park(uint64_t sys, uint64_t, uint64_t, uint64_t) {
    Scope scope;
    return U(Ptr<CFSOnlineSystem>(sys)->GetFTPDownloadParkInfo());
}
uint64_t tpw_crl_f_ftp_path(uint64_t sys, uint64_t, uint64_t, uint64_t) {
    Scope scope;
    return U(Ptr<CFSOnlineSystem>(sys)->ReturnFTPDownloadLocalPath());
}
// ChildLocked of the base of the park, city, theme, server and ride instances (the inline base body; `object` is not read).
uint64_t tpw_crl_f_child_locked(uint64_t which, uint64_t object, uint64_t, uint64_t) {
    Scope scope;
    switch (which) {
        case 0: return static_cast<uint32_t>(Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>>(object)->CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::ChildLocked());
        case 1: return static_cast<uint32_t>(Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>>(object)->CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::ChildLocked());
        case 2: return static_cast<uint32_t>(Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>>(object)->CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::ChildLocked());
        case 3: return static_cast<uint32_t>(Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>>(object)->CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::ChildLocked());
        default: return static_cast<uint32_t>(Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>>(object)->CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::ChildLocked());
    }
}
uint64_t tpw_crl_f_destroy(uint64_t, uint64_t, uint64_t, uint64_t) {
    Scope scope;
    AllocatorUShortDestroy(nullptr, nullptr);
    return 0;
}
uint64_t tpw_crl_f_dtor_city(uint64_t object, uint64_t flag, uint64_t, uint64_t) {
    Scope scope;
    CFSOnlineSystemCityList* list = Ptr<CFSOnlineSystemCityList>(object);
    if (static_cast<int16_t>(flag) > 0) delete list;
    else list->~CFSOnlineSystemCityList();
    return 0;
}
uint64_t tpw_crl_f_dtor_theme(uint64_t object, uint64_t flag, uint64_t, uint64_t) {
    Scope scope;
    CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>* list = Ptr<CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>>(object);
    online_list_base::ThemeListBaseDtor(list);
    if (static_cast<int16_t>(flag) > 0) delete list;   // the empty C++ destructor (unit v51129's) and the free
    return 0;
}
uint64_t tpw_crl_f_pl_park(uint64_t error, uint64_t context, uint64_t, uint64_t) {
    Scope scope;
    PostLoginCallbackFn_Park(static_cast<int32_t>(error), Ptr<void>(context));
    return 0;
}
uint64_t tpw_crl_f_pl_city(uint64_t error, uint64_t context, uint64_t, uint64_t) {
    Scope scope;
    PostLoginCallbackFn_City(static_cast<int32_t>(error), Ptr<void>(context));
    return 0;
}
uint64_t tpw_crl_f_logged_in(uint64_t object, uint64_t, uint64_t, uint64_t) {
    Scope scope;
    Ptr<ParkBase>(object)->LoggedInGetList();
    return 0;
}
uint64_t tpw_crl_f_top10(uint64_t object, uint64_t id, uint64_t callback, uint64_t context) {
    Scope scope;
    Ptr<CFSOnlineSystemParkList_InCityTop10>(object)->GetList(
        static_cast<int32_t>(id), reinterpret_cast<CFSOnlineSystemParkList::Callback>(static_cast<uintptr_t>(callback & 0xFFFFFFFFu)),
        Ptr<void>(context & 0xFFFFFFFFu));
    return 0;
}
uint64_t tpw_crl_f_by_id(uint64_t object, uint64_t id, uint64_t callback, uint64_t context) {
    Scope scope;
    Ptr<CFSOnlineSystemParkList_InWorldByParkID>(object)->GetList(
        static_cast<int32_t>(id), reinterpret_cast<CFSOnlineSystemParkList::Callback>(static_cast<uintptr_t>(callback & 0xFFFFFFFFu)),
        Ptr<void>(context & 0xFFFFFFFFu));
    return 0;
}

}  // extern "C"
