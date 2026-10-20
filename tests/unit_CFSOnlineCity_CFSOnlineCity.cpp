// Test library for the online city, park, server and ride-list record functions (unit v5994; libtpw_unit_CFSOnlineCity_CFSOnlineCity.dylib,
// tests/test_CFSOnlineCity_CFSOnlineCity.py), main app:
//   src/CFSOnlineSystemRideList_RequestListFromServer_CFSOnlineCity_CFSOnlineCity.cpp
//
// The callees outside the unit are recording fakes: the strings' constructor, destructor and virtual assignments, the list calls of the
// city and server records, the ride list's two callees, the online system's constructor, operator new (for the system's size only) and
// operator delete (for the one watched object). gOnlineSystem is defined here.
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

CFSOnlineSystem* gOnlineSystem;

namespace {
bool gQuiet = false;                       // set while the test builds objects: the string fakes log nothing
CFSOnlineSystemRideList* gListForNewSystem = nullptr;
}  // namespace

// ---- operator new / delete: the program's runtime ones are faked; only the two objects the test watches are logged
// (the runtime's operator new is real here: the system's constructor fake logs the request, the city's free goes through a weak callee)
namespace onlinelist_callee {
void FreeCity(void* city) { ev("delete", {lab(city)}); }
}  // namespace onlinelist_callee

// ---- the strings (declared, defined by files this library does not compile)
#define STRING_FAKES(CH, TAG, ...)                                                                                                  \
    template <> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate() {                                                         \
        if (!gQuiet) ev("StrCtor", {newlab(this)});                                                                              \
    }                                                                                                                            \
    template <> TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() {                                                        \
        if (!gQuiet) ev("StrDtor", {lab(this), -1});                                                                             \
    }                                                                                                                            \
    template <> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                                      \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                          \
    template <> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                                       \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; }    \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate& other) {      \
        ev("StrCopyAssign" TAG, {lab(this), lab(const_cast<TbDynamicStringTemplate*>(&other))});                                 \
        return *this;                                                                                                            \
    }                                                                                                                            \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH* text) {                            \
        __VA_ARGS__                                                                                              \
        return *this;                                                                                                            \
    }                                                                                                                            \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }                 \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; }  \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }                \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }

STRING_FAKES(char16_t, "16", ev("StrAssignPtr", {lab(this)}, {narrow(text).c_str()});)
STRING_FAKES(char, "8", (void)text;)

// ---- the list calls of the city and server records (override the weak ones of the translation)
namespace onlinelist_callee {
void CityAddTail(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) { ev("CityAddTail", {lab(list), lab(city)}); }
void* CityFind(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) {
    ev("CityFind", {lab(list), lab(city)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("CityFind")));
}
void CityRemoveAt(TbPtrList<CFSOnlineCity>* list, void*& position) {
    ev("CityRemoveAt", {lab(list), static_cast<int64_t>(reinterpret_cast<uintptr_t>(position))});
}
void* ServerFind(TbPtrList<CFSOnlineServer>* list, CFSOnlineServer* server) {
    ev("ServerFind", {lab(list), lab(server)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("ServerFind")));
}
void ServerRemoveAt(TbPtrList<CFSOnlineServer>* list, void*& position) {
    ev("ServerRemoveAt", {lab(list), static_cast<int64_t>(reinterpret_cast<uintptr_t>(position))});
}
}  // namespace onlinelist_callee

// ---- the ride list's callees and the online system's constructor
template <> void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::CompleteGetList(int32_t error) {
    ev("CompleteGetList", {lab(this), error});
}
template <> void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::LoggedInGetList() { ev("LoggedInGetList", {lab(this)}); }

CFSOnlineSystem::CFSOnlineSystem() {
    ev("new", {0x404});
    ev("SystemCtor", {newlab(this)});
    mRideList = gListForNewSystem;
}

// ---- the server record's constructor (untranslated; the test builds the object with it, quietly)
CFSOnlineServer::CFSOnlineServer(const ccServerInfo*, CFSOnlineSystemServerList* list) { mList = list; }

void CFSOnlineServer_DestructBody(CFSOnlineServer* self);
CFSOnlineServer::~CFSOnlineServer() { CFSOnlineServer_DestructBody(this); }

namespace {
template <class T>
std::int64_t OffsetOf(std::size_t (*member)(T*)) {
    return static_cast<std::int64_t>(member(nullptr));
}
alignas(16) char gProbe[4096];
#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<char*>(&reinterpret_cast<T*>(gProbe)->m) - gProbe)
}  // namespace

extern "C" {

uint64_t tpw_cc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint32_t tpw_cc_off(uint64_t p) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p)))); }

// The sizes and offsets the module relies on, in the order of the module's LAYN.
int32_t tpw_cc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = sizeof(CFSOnlineCity);
    out[n++] = OFF(CFSOnlineCity, mList);
    out[n++] = OFF(CFSOnlineCity, mCityID);
    out[n++] = OFF(CFSOnlineCity, mPosX);
    out[n++] = OFF(CFSOnlineCity, mPosY);
    out[n++] = OFF(CFSOnlineCity, mPosZ);
    out[n++] = OFF(CFSOnlineCity, mValue);
    out[n++] = OFF(CFSOnlineCity, mF58);
    out[n++] = OFF(CFSOnlineCity, mF5C);
    out[n++] = OFF(CFSOnlineCity, mF60);
    out[n++] = OFF(CFSOnlineCity, mF64);
    out[n++] = OFF(CFSOnlineCity, mText0C);
    out[n++] = OFF(CFSOnlineCity, mText20);
    out[n++] = OFF(CFSOnlineCity, mText44);
    out[n++] = sizeof(CFSOnlinePark);
    out[n++] = OFF(CFSOnlinePark, mParkID);
    out[n++] = OFF(CFSOnlinePark, mF48);
    out[n++] = OFF(CFSOnlinePark, mF4C);
    out[n++] = OFF(CFSOnlinePark, mF50);
    out[n++] = OFF(CFSOnlinePark, mF54);
    out[n++] = OFF(CFSOnlinePark, mF58);
    out[n++] = OFF(CFSOnlinePark, mF78);
    out[n++] = OFF(CFSOnlinePark, mF90);
    out[n++] = OFF(CFSOnlinePark, mFA8);
    out[n++] = OFF(CFSOnlinePark, mFAA);
    out[n++] = OFF(CFSOnlinePark, mFAC);
    out[n++] = OFF(CFSOnlinePark, mFB0);
    out[n++] = OFF(CFSOnlinePark, mFB4);
    out[n++] = OFF(CFSOnlinePark, mFB8);
    out[n++] = OFF(CFSOnlinePark, mFBC);
    out[n++] = OFF(CFSOnlinePark, mText0C);
    out[n++] = OFF(CFSOnlinePark, mText20);
    out[n++] = OFF(CFSOnlinePark, mText34);
    out[n++] = OFF(CFSOnlinePark, mText7C);
    out[n++] = OFF(CFSOnlinePark, mText94);
    out[n++] = sizeof(CFSOnlineServer);
    out[n++] = OFF(CFSOnlineServer, mText10);
    out[n++] = OFF(CFSOnlineServer, mText24);
    out[n++] = OFF(CFSOnlineServer, mText40);
    out[n++] = OFF(CFSOnlineServer, mText54);
    out[n++] = OFF(CFSOnlineServer, mText6C);
    out[n++] = sizeof(CFSOnlineSystemCityList);
    out[n++] = sizeof(CFSOnlineSystemServerList);
    out[n++] = sizeof(CFSOnlineSystemRideList);
    out[n++] = sizeof(CFSOnlineSystem);
    return n;
}

// ---- city
void tpw_cc_owner_set(int32_t kind, uint64_t owner, uint64_t table) {
    void* o = reinterpret_cast<void*>(static_cast<uintptr_t>(owner));
    void* t = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
    if (kind == 0)
        static_cast<CFSOnlineSystemCityList*>(o)->mList = static_cast<TbPtrList<CFSOnlineCity>*>(t);
    else
        static_cast<CFSOnlineSystemServerList*>(o)->mList = static_cast<TbPtrList<CFSOnlineServer>*>(t);
}
void tpw_cc_city_set_owner(uint64_t city, uint64_t owner) {
    reinterpret_cast<CFSOnlineCity*>(static_cast<uintptr_t>(city))->mList =
        reinterpret_cast<CFSOnlineSystemCityList*>(static_cast<uintptr_t>(owner));
}
uint64_t tpw_cc_city_owner(uint64_t city) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<CFSOnlineCity*>(static_cast<uintptr_t>(city))->mList));
}
void tpw_cc_city_ctor(uint64_t city, uint64_t info, uint64_t owner) {
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(city)))
        CFSOnlineCity(reinterpret_cast<const ccCityInfo*>(static_cast<uintptr_t>(info)),
                      reinterpret_cast<CFSOnlineSystemCityList*>(static_cast<uintptr_t>(owner)));
}
void tpw_cc_city_init(uint64_t city) {   // the strings made quietly, as the constructor would have (their vtables)
    CFSOnlineCity* c = reinterpret_cast<CFSOnlineCity*>(static_cast<uintptr_t>(city));
    gQuiet = true;
    new (c->mText0C) TbDynamicStringTemplate<char16_t>();
    new (c->mText20) TbDynamicStringTemplate<char16_t>();
    new (c->mText44) TbDynamicStringTemplate<char16_t>();
    gQuiet = false;
}
void tpw_cc_city_dtor(uint64_t city, int32_t flag) {
    CFSOnlineCity* c = reinterpret_cast<CFSOnlineCity*>(static_cast<uintptr_t>(city));
    if (c == nullptr) return;
    c->Destruct(static_cast<int16_t>(flag));
}

// ---- server
void tpw_cc_server_init(uint64_t server, uint64_t owner) {
    gQuiet = true;
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(server)))
        CFSOnlineServer(nullptr, reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(owner)));
    gQuiet = false;
}
void tpw_cc_server_dtor(uint64_t server, int32_t flag) {
    CFSOnlineServer* s = reinterpret_cast<CFSOnlineServer*>(static_cast<uintptr_t>(server));
    if (s == nullptr) return;
    s->~CFSOnlineServer();
    if (static_cast<int16_t>(flag) > 0) ev("delete", {lab(s)});   // the deleting form's operator delete, logged instead of freeing arena memory
}

// ---- park
void tpw_cc_park_init(uint64_t park) {
    CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(park));
    gQuiet = true;
    new (&p->mText0C) TbDynamicStringTemplate<char16_t>();
    new (&p->mText20) TbDynamicStringTemplate<char16_t>();
    new (&p->mText34) TbDynamicStringTemplate<char16_t>();
    new (&p->mText7C) TbDynamicStringTemplate<char>();
    new (&p->mText94) TbDynamicStringTemplate<char16_t>();
    gQuiet = false;
}
uint64_t tpw_cc_park_assign(uint64_t dst, uint64_t src) {
    CFSOnlinePark* d = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(dst));
    const CFSOnlinePark* s = reinterpret_cast<const CFSOnlinePark*>(static_cast<uintptr_t>(src));
    CFSOnlinePark& r = *d = *s;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&r));
}

// ---- ride
uint64_t tpw_cc_sys_make(uint64_t block, uint64_t list) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(block));
    s->mRideList = reinterpret_cast<CFSOnlineSystemRideList*>(static_cast<uintptr_t>(list));
    return block;
}
void tpw_cc_gsys_set(uint64_t sys) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys)); }
uint64_t tpw_cc_gsys_get() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineSystem)); }
void tpw_cc_ride(int32_t error, uint64_t list) {
    gListForNewSystem = reinterpret_cast<CFSOnlineSystemRideList*>(static_cast<uintptr_t>(list));
    RideListPostLoginCallbackFn(error, nullptr);
}

}  // extern "C"
