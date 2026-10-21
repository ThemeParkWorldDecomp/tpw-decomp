// Test library for the online islands' small members and the globes' panel members (unit v51106;
// libtpw_unit_CLBScreenComponentOnlineIslands_StartCurrentIsla.dylib, tests/test_CLBScreenComponentOnlineIslands_StartCurrentIsla.py), main app:
//   src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The virtual calls the
// Mac makes through __ptr_glue are the host's own members: the fakes record "vcall" with the vtable offset. Deleting an object records "free"
// (the global operator delete is replaced: it records the labelled objects and frees nothing). Objects are built in arena blocks.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"
#include "TPSound.h"

using namespace calllog;

// ---- globals of other files, defined here
CLBLobbyMeshes* gLobbyMeshes;
CLBFlyingMeshSystem* gLobbyFlyingMeshes;

void RemoveCityLobbyController();
void ShowOnlineCityInfoOverlay();
void HideOnlineCityInfoOverlay();

// ---- free callees
void ShowCityLobbyController() { ev("ShowCityLobbyController", {}); }
void HideCityLobbyController() { ev("HideCityLobbyController", {}); }
void RemoveCityLobbyController() { ev("RemoveCityLobbyController", {}); }
void ShowOnlineCityInfoOverlay() { ev("ShowOnlineCityInfoOverlay", {}); }
void HideOnlineCityInfoOverlay() { ev("HideOnlineCityInfoOverlay", {}); }
void ShowWorldLobbyController() { ev("ShowWorldLobbyController", {}); }
void HideWorldLobbyController() { ev("HideWorldLobbyController", {}); }
void ShowWorldLobbySats() { ev("ShowWorldLobbySats", {}); }
void HideWorldLobbySats() { ev("HideWorldLobbySats", {}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_Fade", {handle}); }

void CLBLobbyMeshes::HideHeightfield() { ev("HideHeightfield", {lab(this)}); }
void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMeshInstanceListEntries", {lab(this)}); }
void CLBFlyingMeshSystem::DeleteFlyingMeshGroupListEntries() { ev("DeleteFlyingMeshGroupListEntries", {lab(this)}); }

// ---- the members the Mac reaches through its vtables or calls directly
CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() { ev("BaseDtor", {lab(this), 0}); }
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() {
    ev("vcall", {0x2c, lab(this)});
    return static_cast<int32_t>(ret("IsVisible"));
}

CLBScreenComponentIslands::CLBScreenComponentIslands() {}
CLBScreenComponentIslands::~CLBScreenComponentIslands() {}
void CLBScreenComponentIslands::Init() {}
void CLBScreenComponentIslands::Render() {}
void CLBScreenComponentIslands::Close() {}
void CLBScreenComponentIslands::Show() {}
void CLBScreenComponentIslands::Hide() {}
void CLBScreenComponentIslands::GotoNextIsland() { ev("vcall", {0x40, lab(this)}); }
void CLBScreenComponentIslands::GotoPreviousIsland() { ev("vcall", {0x44, lab(this)}); }
void CLBScreenComponentIslands::Slot48() { ev("vcall", {0x48, lab(this)}); }

void CLBScreenComponentOnlineIslands::Tick() {}
uint32_t CLBScreenComponentOnlineIslands::ProcessCancel() { return 0; }
void CLBScreenComponentOnlineIslands::Show() {}
void CLBScreenComponentOnlineIslands::GotoNextIsland() { ev("vcall", {0x40, lab(this)}); }
void CLBScreenComponentOnlineIslands::SelectCurrentIsland() { ev("vcall", {0x48, lab(this)}); }

CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentOnlineGlobe::TickGlobe() { ev("OnlineGlobeTick", {lab(this)}); }

void CLBScreenComponentPublishGlobe::Init() {}
void CLBScreenComponentPublishGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentPublishGlobe::Show() {}
void CLBScreenComponentPublishGlobe::Hide() {}

// The island's string members are built and destroyed by the (faked) constructor and destructor.
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {}
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

static s3DPoint gZeroPoint;
CLBIsland::CLBIsland(const CLBScreenComponentIslands*, int32_t, const char*, const char*, const char*, const char*, float, const s3DPoint&,
                     const char16_t*, const CFSOnlinePark*, const char16_t*, const char16_t*) {}
CLBIsland::~CLBIsland() {}
CLBGlobe::~CLBGlobe() {}
CLBGlobeCityFlag::~CLBGlobeCityFlag() {}

// ---- delete: records the labelled objects (islands, globes, flags, lists, the screens), frees nothing

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<char*>(&reinterpret_cast<T*>(0x1000)->m) - reinterpret_cast<char*>(0x1000))
#define OI(p) reinterpret_cast<CLBScreenComponentOnlineIslands*>(p)
#define PG(p) reinterpret_cast<CLBScreenComponentPublishGlobe*>(p)
#define DG(p) reinterpret_cast<CLBScreenComponentDownloadGlobe*>(p)

typedef TbTList<CLBGlobe*, CLBGlobe*> GlobeList;
typedef TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*> FlagList;

extern "C" {

uint64_t tpw_oi_addr(const char* name) {
    if (std::strcmp(name, "gLobbyMeshes") == 0) return reinterpret_cast<uintptr_t>(&gLobbyMeshes);
    if (std::strcmp(name, "gLobbyFlyingMeshes") == 0) return reinterpret_cast<uintptr_t>(&gLobbyFlyingMeshes);
    return 0;
}

uint64_t tpw_oi_alloc(uint32_t size) {
    // From the global operator new, so that the program's deletes of these objects match (the shim's operator delete frees them).
    unsigned char* p = static_cast<unsigned char*>(::operator new(size));
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

// Host offsets, in the order of the module's LAYOUT_NAMES.
int32_t tpw_oi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(CLBScreenComponentOnlineIslands, mVisible);
    out[n++] = OFF(CLBScreenComponentOnlineIslands, mIslands);
    out[n++] = OFF(CLBScreenComponentOnlineIslands, mCurrent);
    out[n++] = OFF(CLBScreenComponentOnlineIslands, mCameraState);
    out[n++] = OFF(CLBScreenComponentOnlineIslands, mFE8);
    out[n++] = OFF(FlagList, mCount);
    out[n++] = OFF(FlagList, mHead);
    out[n++] = OFF(FlagList, mTail);
    out[n++] = OFF(FlagList::CNode, mNext);
    out[n++] = OFF(FlagList::CNode, mPrev);
    out[n++] = OFF(CLBIsland, mSound1);
    out[n++] = OFF(CLBIsland, mSound2);
    out[n++] = OFF(CLBScreenComponentOnlineGlobe, mGlobes);
    out[n++] = OFF(CLBScreenComponentOnlineGlobe, mCityFlags);
    out[n++] = OFF(CFSOnlineCity, mValue);
    out[n++] = OFF(CFSOnlineCity, mF60);
    out[n++] = OFF(CFSOnlineCity, mF64);
    return n;
}

// Builds an object of a kind in a block: 0 online islands, 1 island, 2 globe, 3 city flag, 4 island list, 5 globe list, 6 flag list.
void tpw_oi_new(int32_t kind, uint64_t p) {
    void* m = reinterpret_cast<void*>(p);
    switch (kind) {
        case 0: new (m) CLBScreenComponentOnlineIslands; break;
        case 1: new (m) CLBIsland(nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0.0f, gZeroPoint, nullptr, nullptr, nullptr, nullptr); break;
        case 2: new (m) CLBGlobe; break;
        case 3: new (m) CLBGlobeCityFlag; break;
        case 4: new (m) TbPtrList<CLBIsland>(); break;
        case 5: new (m) GlobeList(); break;
        case 6: new (m) FlagList(); break;
    }
}

void tpw_oi_addtail(int32_t kind, uint64_t list, uint64_t item) {
    void* l = reinterpret_cast<void*>(list);
    if (kind == 4) static_cast<TbPtrList<CLBIsland>*>(l)->AddTail(reinterpret_cast<CLBIsland*>(item));
    else if (kind == 5) static_cast<GlobeList*>(l)->AddTail(reinterpret_cast<CLBGlobe*>(item));
    else if (kind == 6) static_cast<FlagList*>(l)->AddTail(reinterpret_cast<CLBGlobeCityFlag*>(item));
}

// ---- the functions: (object, x, y, z)
uint64_t tpw_oi_zoom(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->StartCurrentIslandZoom(); return 0; }
uint64_t tpw_oi_setcur(uint64_t p, int64_t x, int64_t, int64_t) { OI(p)->SetCurrentIsland(reinterpret_cast<CLBIsland*>(x)); return 0; }
uint64_t tpw_oi_prev(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->GotoPreviousIsland(); return 0; }
uint64_t tpw_oi_hidep(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->HidePanels(); return 0; }
uint64_t tpw_oi_showp(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->ShowPanels(); return 0; }
uint64_t tpw_oi_showinfo(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->ShowInfoOverlayPanel(); return 0; }
uint64_t tpw_oi_hide(uint64_t p, int64_t, int64_t, int64_t) { OI(p)->Hide(); return 0; }
uint64_t tpw_oi_dtor(uint64_t p, int64_t x, int64_t, int64_t) {
    if (p == 0) return 0;
    OI(p)->CLBScreenComponentOnlineIslands::~CLBScreenComponentOnlineIslands();
    if (static_cast<int16_t>(x) > 0) ::operator delete(reinterpret_cast<void*>(p));
    return p;
}
uint64_t tpw_oi_wmf(uint64_t p, int64_t x, int64_t y, int64_t z) {
    OI(p)->CLBScreenComponentOnlineIslands::WindowMessagesFn(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
    return 0;
}
uint64_t tpw_oi_wmfi(uint64_t p, int64_t x, int64_t y, int64_t z) {
    static_cast<CLBScreenComponentIslands*>(OI(p))->WindowMessagesFnIslands(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
    return 0;
}
uint64_t tpw_oi_pdtor(uint64_t p, int64_t x, int64_t, int64_t) {
    if (p == 0) return 0;
    PG(p)->CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe();
    if (static_cast<int16_t>(x) > 0) ::operator delete(reinterpret_cast<void*>(p));
    return p;
}
uint64_t tpw_oi_picv(uint64_t p, int64_t x, int64_t, int64_t) {
    return static_cast<uint32_t>(PG(p)->CLBScreenComponentPublishGlobe::IsCityValid(reinterpret_cast<const CFSOnlineCity*>(x)));
}
uint64_t tpw_oi_phide(uint64_t p, int64_t, int64_t, int64_t) { PG(p)->CLBScreenComponentPublishGlobe::HidePanels(); return 0; }
uint64_t tpw_oi_pshow(uint64_t p, int64_t, int64_t, int64_t) { PG(p)->CLBScreenComponentPublishGlobe::ShowPanels(); return 0; }
uint64_t tpw_oi_ptick(uint64_t p, int64_t, int64_t, int64_t) { PG(p)->CLBScreenComponentPublishGlobe::Tick(); return 0; }
uint64_t tpw_oi_dicv(uint64_t p, int64_t x, int64_t, int64_t) {
    return static_cast<uint32_t>(DG(p)->CLBScreenComponentDownloadGlobe::IsCityValid(reinterpret_cast<const CFSOnlineCity*>(x)));
}
uint64_t tpw_oi_dhide(uint64_t p, int64_t, int64_t, int64_t) { DG(p)->CLBScreenComponentDownloadGlobe::HidePanels(); return 0; }
uint64_t tpw_oi_dshow(uint64_t p, int64_t, int64_t, int64_t) { DG(p)->CLBScreenComponentDownloadGlobe::ShowPanels(); return 0; }
uint64_t tpw_oi_dtick(uint64_t p, int64_t, int64_t, int64_t) { DG(p)->CLBScreenComponentDownloadGlobe::Tick(); return 0; }
uint64_t tpw_oi_reg(uint64_t p, int64_t x, int64_t, int64_t) {
    reinterpret_cast<CLBScreenComponentOnlineGlobe*>(p)->Register(reinterpret_cast<CLBGlobeCityFlag*>(x));
    return 0;
}

}  // extern "C"
