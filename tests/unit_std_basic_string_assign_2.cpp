// Test library for unit v51129 (libtpw_unit_std_basic_string_assign_2.dylib, tests/test_std_basic_string_assign_2.py), main app:
//   src/game/MslSupport.cpp, src/game/online/FS_OnlineList.cpp, src/game/ui/InterfaceDrawEngine.cpp   LCString::assign, InterfaceDrawEngineHGradient::~, CFSOnlineSystemRideList::GetList,
//   the server list's base destructor, the theme list's destructor and LoggedInGetList.
//
// The callees outside the unit are fakes that record what the real ones read (the PowerPC stubs of the module record the same events) and answer
// from the test's queues. The classes whose constructors and destructors live in other files get minimal definitions here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "ui_controls.h"
#include "ui_ime.h"
#include "std_basic_streambuf_wchar.h"
#include "ui_DrawObject.h"
#include "TPEngine.h"

using namespace calllog;

using ServerBase = CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>;
using ThemeBase = CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>;
using RideBase = CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>;

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem = nullptr;
static void CallbackCaller(int32_t callback, int32_t error, int32_t context, void* list) {
    ev("callback", {callback, error, context, lab(list)});
}
void (*gOnlineListCallbackCaller)(int32_t, int32_t, int32_t, void*) = CallbackCaller;

namespace {
void* gElem[3];
void* gCity = nullptr;
void* gInst = nullptr;
void* gAltRep = nullptr;
}  // namespace

// ---- strings: change_size (may replace the representation) and wmemcpy
namespace IME {
void MslWideString_changeSize(MslWideString* self, uint32_t n, bool keep) {
    ev("changeSize", {lab(self), n, keep ? 1 : 0});
    if (ret("swap") != 0) reinterpret_cast<LCString*>(self)->mRep = reinterpret_cast<LCStringRep*>(gAltRep);
}
}  // namespace IME
char16_t* wmemcpy(char16_t* dst, const char16_t* src, size_t n) {
    ev("wmemcpy", {lab(dst), lab(src), static_cast<int64_t>(n)});
    return dst;
}

// ---- the gradient: the base class and the mesh calls
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("ObjDtor", {lab(this), 0}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
void InterfaceDrawEngineHGradient::Change(uint32_t, DOChange*) {}
void InterfaceDrawEngineHGradient::Show(int16_t) {}
void DMFreeTextures(sDTextureSet* set) { ev("DMFreeTextures", {lab(set)}); }
int32_t DMFreeMesh(tag_sDMeshSet* set) {
    ev("DMFreeMesh", {lab(set)});
    return 0;
}

// ---- the online system
CFSOnlineSystem::CFSOnlineSystem() { ev("SysCtor", {lab(this)}); }
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
void RideListPostLoginCallbackFn(int32_t error, const void* context) { ev("RideCb", {error, lab(context)}); }
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this), callback == RideListPostLoginCallbackFn ? 1 : 0, lab(context)});
}
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    ev("Instance", {});
    return static_cast<CFSOnlineSystem*>(gInst);
}
City* CFSOnlineSystem::ReturnConnectionToCityServer() {
    ev("ReturnConn", {lab(this)});
    return ret("ReturnConn") != 0 ? static_cast<City*>(gCity) : nullptr;
}
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }

// ---- the lists' callees
ccThemeInfo::~ccThemeInfo() {}
namespace onlinelist2_callee {
void DestroyRideInfos(ccRideInfo* infos) { ev("DestroyInfos", {lab(infos), 0}); }
void DestroyServerInfos(ccServerInfo* infos) { ev("DestroyInfos", {lab(infos), 1}); }
void FreeBlock(void* block) { ev("FreeBlock", {lab(block)}); }
CFSOnlineServer* ServerRemoveHead(TbPtrList<CFSOnlineServer>* list) {
    ev("RemoveHead", {lab(list)});
    int64_t k = ret("RemoveHead");
    --list->mCount;
    return k > 0 ? static_cast<CFSOnlineServer*>(gElem[k - 1]) : nullptr;
}
void DestroyServer(CFSOnlineServer* server) { ev("vdtor", {lab(server), 1}); }
void DestroyServerList(TbPtrList<CFSOnlineServer>* list) { ev("vdtor", {lab(list), 1}); }
}  // namespace onlinelist2_callee
namespace onlinelist3_callee {
void DestroyThemeInfos(ccThemeInfo* infos) { ev("DestroyInfos", {lab(infos), 2}); }
CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list) {
    ev("RemoveHead", {lab(list)});
    int64_t k = ret("RemoveHead");
    --list->mCount;
    return k > 0 ? static_cast<CFSOnlineTheme*>(gElem[k - 1]) : nullptr;
}
void DestroyTheme(CFSOnlineTheme* theme) { ev("vdtor", {lab(theme), 1}); }
void DestroyThemeList(TbPtrList<CFSOnlineTheme>* list) { ev("vdtor", {lab(list), 1}); }
}  // namespace onlinelist3_callee

// ---- the classes whose members live in other files
CFSOnlineSystemThemeList::CFSOnlineSystemThemeList() {}
CFSOnlineSystemRideList::CFSOnlineSystemRideList() {}
CFSOnlineSystemRideList::~CFSOnlineSystemRideList() {}
template<> RideBase::~CFSOnlineSystemList() {}

namespace {
struct TestTheme : CFSOnlineSystemThemeList {
    int32_t ChildLocked() override {
        ev("ChildLocked", {lab(this)});
        return static_cast<int32_t>(ret("ChildLocked"));
    }
    int32_t RequestListFromServer(City* city, uint32_t arg) override {
        ev("RequestListFromServer", {lab(this), lab(city), arg});
        return static_cast<int32_t>(ret("RequestListFromServer"));
    }
};
}  // namespace

extern "C" {

uint64_t tpw_sa2_addr(const char*) { return 0; }

uint64_t tpw_sa2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sa2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(LCStringRep, mData));
    return n;
}

// ---- strings: a string is one pointer; a representation is length, capacity, references, data
void tpw_sa2_str_set(uint64_t str, uint64_t rep) { reinterpret_cast<LCString*>(str)->mRep = reinterpret_cast<LCStringRep*>(rep); }
uint64_t tpw_sa2_str_get(uint64_t str) { return reinterpret_cast<uintptr_t>(reinterpret_cast<LCString*>(str)->mRep); }
void tpw_sa2_rep_set(uint64_t rep, int32_t len, int32_t cap, int32_t refs, uint64_t data) {
    LCStringRep* r = reinterpret_cast<LCStringRep*>(rep);
    r->mLength = len;
    r->mCapacity = cap;
    r->mRefCount = refs;
    r->mData = reinterpret_cast<char16_t*>(data);
}
uint64_t tpw_sa2_rep_get(uint64_t rep, int32_t index) {
    LCStringRep* r = reinterpret_cast<LCStringRep*>(rep);
    switch (index) {
        case 0: return static_cast<uint32_t>(r->mLength);
        case 1: return static_cast<uint32_t>(r->mCapacity);
        case 2: return static_cast<uint32_t>(r->mRefCount);
        default: return reinterpret_cast<uintptr_t>(r->mData);
    }
}
void tpw_sa2_set_alt(uint64_t rep) { gAltRep = reinterpret_cast<void*>(rep); }
uint32_t tpw_sa2_assign(uint64_t str, uint64_t text, uint32_t length) {
    reinterpret_cast<LCString*>(str)->assign(reinterpret_cast<const char16_t*>(text), length);
    return 1;
}

// ---- the gradient
uint64_t tpw_sa2_hg_make() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(sizeof(InterfaceDrawEngineHGradient)))); }
void tpw_sa2_hg_info(uint64_t obj, int64_t* out) {
    char* base = reinterpret_cast<char*>(obj);
    InterfaceDrawEngineHGradient* g = reinterpret_cast<InterfaceDrawEngineHGradient*>(obj);
    out[0] = reinterpret_cast<char*>(&g->mTextures) - base;
    out[1] = reinterpret_cast<char*>(&g->mMeshSet) - base;
    out[2] = reinterpret_cast<char*>(&g->mData) - base;
}
void tpw_sa2_hg_set_data(uint64_t obj, uint64_t value) { reinterpret_cast<InterfaceDrawEngineHGradient*>(obj)->mData = reinterpret_cast<void*>(value); }
uint64_t tpw_sa2_hg_get_data(uint64_t obj) { return reinterpret_cast<uintptr_t>(reinterpret_cast<InterfaceDrawEngineHGradient*>(obj)->mData); }
void tpw_sa2_hg_dtor(uint64_t obj) { reinterpret_cast<InterfaceDrawEngineHGradient*>(obj)->InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient(); }

// ---- the lists. kind 0: the server list's base, 1: the theme list, 2: the ride list.
uint64_t tpw_sa2_make(int32_t kind) {
    void* mem = tpw::ArenaAllocZeroed(256);
    if (kind == 0) new (mem) ServerBase();
    else if (kind == 1) new (mem) TestTheme();
    else new (mem) CFSOnlineSystemRideList();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(mem));
}
void tpw_sa2_set_field(uint64_t obj, int32_t off, int32_t v) {
    ServerBase* o = reinterpret_cast<ServerBase*>(obj);
    int32_t* f[] = {&o->mRequestID, &o->mF08, &o->mF0C, &o->mF10, &o->mF14, &o->mF18, &o->mF1C, &o->mF20, &o->mF24, &o->mF28};
    *f[off / 4 - 1] = v;
}
int32_t tpw_sa2_get_field(uint64_t obj, int32_t off) {
    ServerBase* o = reinterpret_cast<ServerBase*>(obj);
    int32_t* f[] = {&o->mRequestID, &o->mF08, &o->mF0C, &o->mF10, &o->mF14, &o->mF18, &o->mF1C, &o->mF20, &o->mF24, &o->mF28};
    return *f[off / 4 - 1];
}
uint64_t tpw_sa2_get_list(uint64_t obj) { return reinterpret_cast<uintptr_t>(reinterpret_cast<ServerBase*>(obj)->mList); }
void tpw_sa2_set_count(uint64_t list, int32_t n) { reinterpret_cast<TbPtrList<CFSOnlineServer>*>(list)->mCount = n; }
int32_t tpw_sa2_get_count(uint64_t list) { return reinterpret_cast<TbPtrList<CFSOnlineServer>*>(list)->mCount; }
void tpw_sa2_set_recv(uint64_t obj, uint64_t recv) { reinterpret_cast<ServerBase*>(obj)->mReceived = reinterpret_cast<ccServerList*>(recv); }
uint64_t tpw_sa2_get_recv(uint64_t obj) { return reinterpret_cast<uintptr_t>(reinterpret_cast<ServerBase*>(obj)->mReceived); }
void tpw_sa2_recv_set(uint64_t recv, int32_t count, uint64_t infos, int32_t f08, int32_t refs) {
    ccServerList* r = reinterpret_cast<ccServerList*>(recv);
    r->mCount = count;
    r->mInfos = reinterpret_cast<ccServerInfo*>(infos);
    r->mF08 = f08;
    r->mRefCount = refs;
}
uint64_t tpw_sa2_recv_get(uint64_t recv, int32_t index) {
    ccServerList* r = reinterpret_cast<ccServerList*>(recv);
    switch (index) {
        case 0: return static_cast<uint32_t>(r->mCount);
        case 1: return reinterpret_cast<uintptr_t>(r->mInfos);
        case 2: return static_cast<uint32_t>(r->mF08);
        default: return static_cast<uint32_t>(r->mRefCount);
    }
}
void tpw_sa2_set_elem(int32_t i, uint64_t p) { gElem[i] = reinterpret_cast<void*>(p); }
void tpw_sa2_set_city(uint64_t p) { gCity = reinterpret_cast<void*>(p); }
void tpw_sa2_set_inst(uint64_t p) { gInst = reinterpret_cast<void*>(p); }
void tpw_sa2_set_sys(uint64_t p) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(p); }
uint64_t tpw_sa2_get_sys() { return reinterpret_cast<uintptr_t>(gOnlineSystem); }
// op 0: the destructor (explicit call, no delete), 1: GetList, 2: LoggedInGetList
void tpw_sa2_call(int32_t kind, int32_t op, uint64_t obj) {
    if (op == 0) {
        if (kind == 0) reinterpret_cast<ServerBase*>(obj)->~ServerBase();
        else reinterpret_cast<TestTheme*>(obj)->~TestTheme();
    } else if (op == 1) {
        reinterpret_cast<CFSOnlineSystemRideList*>(obj)->GetList();
    } else {
        reinterpret_cast<ThemeBase*>(obj)->LoggedInGetList();
    }
}

}  // extern "C"
