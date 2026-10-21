// Test library for the publish and download globes' members (unit v5957; libtpw_unit_CLBScreenComponentPublishGlobe_WindowMessagesFn.dylib,
// tests/test_CLBScreenComponentPublishGlobe_WindowMessagesFn.py), main app:
//   src/game/online/lobby_globe.cpp   Publish: WindowMessagesFn, Show, Hide, CreateCities,
//   HandleOnlineBacktrack; Download: CreateCities, HandleOnlineBacktrack
//
// Every callee outside the unit is a recording fake with the PowerPC stub's event name and arguments; the virtual members the
// functions reach (IsVisible, IsCityWanted, the theme islands' Show and EnterCurrentIsland, the download globe's Hide) are fakes
// too. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "tb_resource_display.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files, defined here
CLBLobbyMeshes* gLobbyMeshes;
CLBScreenSystem* gLobbyScreens;
CFSOnlineSystem* gOnlineSystem;
CDisplay gei_Display;
OnlineParkSelection* gOnlineParkSelection;

CDisplay::CDisplay() {}

namespace {
CFSOnlineSystem* gSysInitial;
}  // namespace

// ---- the base classes' members (trivial, so that the vtables link)
CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() { ev("OnlineGlobeShow", {lab(this)}); }
void CLBScreenComponent::Hide() { ev("OnlineGlobeHide", {lab(this)}); }
int32_t CLBScreenComponent::IsVisible() {
    ev("IsVisible", {lab(this)});
    return static_cast<int32_t>(ret("IsVisible"));
}
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentOnlineGlobe_WindowMessagesFn(CLBScreenComponentOnlineGlobe* globe, int32_t message, int32_t a, int32_t b) {
    ev("OnlineGlobeWindowMessagesFn", {lab(globe), message, a, b});
}
CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe() {}
void CLBScreenComponentPublishGlobe::Init() {}
void CLBScreenComponentPublishGlobe::Tick() {}
CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe() {}
void CLBScreenComponentDownloadGlobe::Init() {}
void CLBScreenComponentDownloadGlobe::Tick() {}
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentDownloadGlobe::Show() {}
void CLBScreenComponentDownloadGlobe::Hide() { ev("DownloadHide", {lab(this)}); }

CLBScreenComponentIslands::CLBScreenComponentIslands() {}
CLBScreenComponentIslands::~CLBScreenComponentIslands() {}
void CLBScreenComponentIslands::Init() {}
void CLBScreenComponentIslands::Render() {}
void CLBScreenComponentIslands::Close() {}
void CLBScreenComponentIslands::Show() {}
void CLBScreenComponentIslands::Hide() {}
CLBScreenComponentThemeIslands::~CLBScreenComponentThemeIslands() {}
void CLBScreenComponentThemeIslands::Tick() {}
void CLBScreenComponentThemeIslands::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() { return 0; }
void CLBScreenComponentThemeIslands::Show() { ev("ThemeShow", {lab(this)}); }
void CLBScreenComponentThemeIslands::Hide() {}
void CLBScreenComponentThemeIslands::EnterCurrentIsland() { ev("ThemeEnter", {lab(this)}); }

// ---- the callees outside the unit
void CreateWorldLobbyController(bool publish) { ev("CreateWorldLobbyController", {publish ? 1 : 0}); }
void CreateOnlineCityInfoOverlay() { ev("CreateOnlineCityInfoOverlay", {}); }
void RemoveOnlineCityInfoOverlay() { ev("RemoveOnlineCityInfoOverlay", {}); }
void RemoveWorldLobbyController() { ev("RemoveWorldLobbyController", {}); }
void CreatePublishParkPanel() { ev("CreatePublishParkPanel", {lab(gOnlineParkSelection)}); }
void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    ev("Add", {lab(this), response.mId, clearFirst});
}
void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMeshInstanceListEntries", {lab(this)}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }
void CFSOnlineSystem::SetPublishing(int32_t publishing, const char* text, const uint16_t* words) {
    ev("SetPublishing", {lab(this), publishing, static_cast<int64_t>(reinterpret_cast<uintptr_t>(text)),
                         static_cast<int64_t>(reinterpret_cast<uintptr_t>(words))});
}
CLBGlobeCityFlag::CLBGlobeCityFlag(const CLBScreenComponentOnlineGlobe* globe, const CFSOnlineCity* city) {
    ev("CLBGlobeCityFlag", {newlab(this), lab(globe), lab(city)});
}
CLBGlobeCityFlag::~CLBGlobeCityFlag() { ev("delete", {lab(this), 1}); }
CLBGlobe::~CLBGlobe() { ev("delete", {lab(this), 1}); }

// The lists' RemoveHead is the generic one of TbTList.h (inline), on real nodes the test builds.

// ---- test classes: the cities the globes want come from the case's queue
namespace {
struct TestPublish : CLBScreenComponentPublishGlobe {
    int32_t IsCityWanted(CFSOnlineCity* city) override {
        ev("IsCityWanted", {lab(this), lab(city)});
        return static_cast<int32_t>(ret("IsCityWanted"));
    }
};
struct TestDownload : CLBScreenComponentDownloadGlobe {
    int32_t IsCityWanted(CFSOnlineCity* city) override {
        ev("IsCityWanted", {lab(this), lab(city)});
        return static_cast<int32_t>(ret("IsCityWanted"));
    }
};
struct ListBlock {   // a TbTList's words: vtable, head, tail, count (the fake RemoveHead reads only the count)
    void* vtable;
    void* head;
    void* tail;
    int32_t count;
};

uint64_t P(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
template <class T>
T* Arena() {
    return new (tpw::ArenaAllocZeroed(sizeof(T))) T();
}
}  // namespace

extern "C" {

uint64_t tpw_pg_addr(const char*) { return 0; }
uint64_t tpw_pg_alloc(uint32_t size) { return P(tpw::ArenaAllocZeroed(size)); }
int32_t tpw_pg_layout(int64_t*) { return 0; }

void tpw_pg_reset() {
    gLobbyScreens = nullptr;
    gLobbyMeshes = nullptr;
    gOnlineSystem = nullptr;
    gSysInitial = nullptr;
    gei_Display.mFlags = 0;
    gOnlineParkSelection = nullptr;
}

// what: 0 publish globe, 1 download globe, 2 globe list, 3 flag list, 4 pool globe, 5 pool flag, 6 city, 7 screens, 8 advisor,
// 9 theme islands, 10 meshes, 11 online system
uint64_t tpw_pg_make(int32_t what) {
    switch (what) {
        case 0: return P(Arena<TestPublish>());
        case 1: return P(Arena<TestDownload>());
        case 2:
        case 3: return P(tpw::ArenaAllocZeroed(sizeof(ListBlock)));
        case 4: return P(new CLBGlobe());
        case 5: return P(new CLBGlobeCityFlag());
        case 6: return P(new CFSOnlineCity());
        case 7: return P(tpw::ArenaAllocZeroed(sizeof(CLBScreenSystem)));
        case 8: return P(tpw::ArenaAllocZeroed(sizeof(CLBScreenComponentAdvisor)));
        case 9: return P(Arena<CLBScreenComponentThemeIslands>());
        case 10: return P(tpw::ArenaAllocZeroed(sizeof(CLBLobbyMeshes)));
        case 11: return P(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
        case 12: return P(Arena<CLBGlobeCityFlag>());   // 32-bit addressable: the globe's word at 0x70 holds it
    }
    return 0;
}

void tpw_pg_link_globe(uint64_t globe, uint64_t globes, uint64_t flags) {
    CLBScreenComponentOnlineGlobe* g = reinterpret_cast<CLBScreenComponentOnlineGlobe*>(globe);
    g->mGlobes = reinterpret_cast<TbTList<CLBGlobe*, CLBGlobe*>*>(globes);
    g->mCityFlags = reinterpret_cast<TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>*>(flags);
}
void tpw_pg_link_flag(uint64_t flag, uint64_t city) {
    reinterpret_cast<CLBGlobeCityFlag*>(flag)->mCity = reinterpret_cast<CFSOnlineCity*>(city);
}
void tpw_pg_link_screens(uint64_t screens, uint64_t theme, uint64_t advisor) {
    CLBScreenSystem* s = reinterpret_cast<CLBScreenSystem*>(screens);
    s->mThemeIslands = reinterpret_cast<CLBScreenComponentThemeIslands*>(theme);
    s->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(advisor);
}

// fields of the globe: 0 mVisible, 1 mF70, 2 mF74, 3 mF78, 4 the word at Mac 0x68, 5 globes' count, 6 flags' count
void tpw_pg_set(uint64_t obj, int32_t field, int64_t value) {
    CLBScreenComponentOnlineGlobe* g = reinterpret_cast<CLBScreenComponentOnlineGlobe*>(obj);
    int32_t v = static_cast<int32_t>(value);
    if (field == 1 && value != 0) v = static_cast<int32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(value)));   // the field holds an arena offset
    switch (field) {
        case 0: g->mVisible = v; break;
        case 1: g->mF70 = v; break;
        case 2: g->mF74 = v; break;
        case 3: g->mF78 = v; break;
        case 4: std::memcpy(g->mF44 + 0x24, &v, 4); break;
    }
}
int64_t tpw_pg_get(uint64_t obj, int32_t field) {
    CLBScreenComponentOnlineGlobe* g = reinterpret_cast<CLBScreenComponentOnlineGlobe*>(obj);
    int32_t v = 0;
    switch (field) {
        case 0: return g->mVisible;
        case 1: return g->mF70;
        case 2: return g->mF74;
        case 3: return g->mF78;
        case 4: std::memcpy(&v, g->mF44 + 0x24, 4); return v;
        case 5: return g->mGlobes->mCount;
        case 6: return g->mCityFlags->mCount;
    }
    return 0;
}

// The globals: the screens, the meshes, the online system (the initial one, or none) and the display flags.
void tpw_pg_globals(uint64_t screens, uint64_t meshes, uint64_t sys, uint32_t flags, uint64_t selection) {
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(screens);
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(meshes);
    gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(sys);
    gSysInitial = gOnlineSystem;
    gei_Display.mFlags = flags;
    gOnlineParkSelection = reinterpret_cast<OnlineParkSelection*>(selection);
}
// 0 the display flags, 1 the online system (0 none, 1 the initial one, 2 another), 2 the park selection pointer
uint64_t tpw_pg_getglob(int32_t which) {
    switch (which) {
        case 0: return gei_Display.mFlags;
        case 1: return gOnlineSystem == nullptr ? 0 : (gOnlineSystem == gSysInitial ? 1 : 2);
        case 2: return P(gOnlineParkSelection);
    }
    return 0;
}

extern "C++" {
template <class L, class T>
void AddNode(L* list, T* data) {
    typename L::CNode* node = new typename L::CNode(data);
    node->mPrev = list->mTail;
    if (list->mTail != nullptr) list->mTail->mNext = node; else list->mHead = node;
    list->mTail = node;
    list->mCount++;
}
}
// A node at the tail of the globe list (which 0) or the flag list (which 1).
void tpw_pg_node(int32_t which, uint64_t list, uint64_t data) {
    if (which == 0)
        AddNode(reinterpret_cast<TbTList<CLBGlobe*, CLBGlobe*>*>(list), reinterpret_cast<CLBGlobe*>(data));
    else
        AddNode(reinterpret_cast<TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>*>(list), reinterpret_cast<CLBGlobeCityFlag*>(data));
}

// A TbPtrList<CFSOnlineCity> of the given cities.
uint64_t tpw_pg_list(int32_t n, const uint64_t* cities) {
    TbPtrList<CFSOnlineCity>* list = reinterpret_cast<TbPtrList<CFSOnlineCity>*>(tpw::ArenaAllocZeroed(sizeof(ListBlock)));
    TbPtrList<CFSOnlineCity>::CNode* prev = nullptr;
    for (int32_t i = 0; i < n; i++) {
        TbPtrList<CFSOnlineCity>::CNode* node = new TbPtrList<CFSOnlineCity>::CNode(reinterpret_cast<CFSOnlineCity*>(cities[i]));
        node->mPrev = prev;
        if (prev != nullptr) prev->mNext = node; else list->mHead = node;
        prev = node;
    }
    list->mTail = prev;
    list->mCount = n;
    return P(list);
}

void tpw_pg_wmf(uint64_t obj, int32_t m, int32_t a, int32_t b) {
    reinterpret_cast<CLBScreenComponentPublishGlobe*>(obj)->WindowMessagesFn(m, a, b);
}
void tpw_pg_show(uint64_t obj) { reinterpret_cast<CLBScreenComponentPublishGlobe*>(obj)->Show(); }
void tpw_pg_hide(uint64_t obj) { reinterpret_cast<CLBScreenComponentPublishGlobe*>(obj)->Hide(); }
void tpw_pg_pcc(uint64_t obj, uint64_t list) {
    reinterpret_cast<CLBScreenComponentPublishGlobe*>(obj)->CreateCities(reinterpret_cast<TbPtrList<CFSOnlineCity>*>(list));
}
void tpw_pg_pbt(uint64_t obj) { reinterpret_cast<CLBScreenComponentPublishGlobe*>(obj)->HandleOnlineBacktrack(); }
void tpw_pg_dcc(uint64_t obj, uint64_t list) {
    reinterpret_cast<CLBScreenComponentDownloadGlobe*>(obj)->CreateCities(reinterpret_cast<TbPtrList<CFSOnlineCity>*>(list));
}
void tpw_pg_dbt(uint64_t obj) { reinterpret_cast<CLBScreenComponentDownloadGlobe*>(obj)->HandleOnlineBacktrack(); }

}  // extern "C"
