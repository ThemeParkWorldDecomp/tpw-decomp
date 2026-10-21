// Test library for the online globe's Show, Hide, zoom, CreateCities and UnRegister (unit v5923;
// libtpw_unit_CLBScreenComponentOnlineGlobe_Show_2.dylib, tests/test_CLBScreenComponentOnlineGlobe_Show_2.py), main app:
//   src/game/online/lobby_globe.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read. The globe object is a TestGlobe: the real class
// is abstract, and its virtuals (IsVisible, RegisterGlobe, IsCityWanted) are fakes that record and answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TPSound.h"
#include "tb_rain.h"
#include "tb_resource_display.h"

using namespace calllog;

// ---- globals of other files, defined here
CLBLobby* gLobby;
CLBLobbyView* gLobbyView;
CLBLobbyMeshes* gLobbyMeshes;
CLBScreenSystem* gLobbyScreens;
CFSOnlineSystem* gOnlineSystem;
int32_t gLobbyThunderBank;
CRain* gei_Rain;
CDisplay gei_Display;
CDisplay::CDisplay() {}

static CLBMeshInstance* gFakeMesh;
static CFSOnlineSystemCityList* gFakeCityList;

// ---- the base classes' members the vtables name
CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return 0; }
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
CLBGlobeCityFlag::~CLBGlobeCityFlag() {}

void CLBScreenComponentOnlineGlobe::DeleteGlobeListEntries() { ev("DeleteGlobeListEntries", {lab(this)}); }
void CLBScreenComponentOnlineGlobe::DeleteCityListEntries() { ev("DeleteCityListEntries", {lab(this)}); }

namespace {
class TestGlobe : public CLBScreenComponentOnlineGlobe {
public:
    void Init() override {}
    void Tick() override {}
    void WindowMessagesFn(int32_t, int32_t, int32_t) override {}
    int32_t IsVisible() override {
        ev("IsVisible", {lab(this)});
        return static_cast<int32_t>(ret("IsVisible"));
    }
    void RegisterGlobe(CLBGlobe* globe) override { ev("RegisterGlobe", {lab(this), lab(globe)}); }
    int32_t IsCityWanted(CFSOnlineCity* city) override {
        ev("IsCityWanted", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(city) & 0xffffffffu)});
        return static_cast<int32_t>(ret("IsCityWanted"));
    }
};
}  // namespace

// ---- the fakes
void CRain::SetRaindrops(int32_t n) { ev("SetRaindrops", {lab(this), n}); }
void CLBLobbyView::SetFogOff() { ev("SetFogOff", {lab(this)}); }
void CLBLobbyView::SetGlobalAmbientLight(int32_t enabled, uint32_t color) { ev("SetGlobalAmbientLight", {lab(this), enabled, color}); }
void CLBLobbyView::SetGlobalDirectionalLight(int32_t enabled, const uint32_t* color, const s3DPoint* normal, int32_t follows) {
    ev("SetGlobalDirectionalLight", {lab(this), enabled, color ? static_cast<int64_t>(*color) : -7,
                                     static_cast<int64_t>(reinterpret_cast<uintptr_t>(normal) & 0xffffffffu), follows});
}
int32_t SOUND_SendEvent(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    ev("SOUND_SendEvent", {a, b, c, d, e, f});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
CLBMeshInstance* CLBLobbyMeshes::InstanceMeshType(const char* a, const char* b, int32_t c, uint32_t d, const char16_t* e, const char16_t* f) {
    ev("InstanceMeshType", {lab(this), c, static_cast<int64_t>(d), static_cast<int64_t>(reinterpret_cast<uintptr_t>(e) & 0xffffffffu),
                            static_cast<int64_t>(reinterpret_cast<uintptr_t>(f) & 0xffffffffu)},
       {a, b});
    return gFakeMesh;
}
void CLBMeshInstance::Show() { ev("MeshShow", {lab(this)}); }
void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMeshInstanceListEntries", {lab(this)}); }
CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem", {newlab(this)});
    mCityList = gFakeCityList;
}
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
void CFSOnlineSystemCityList::GetList(OnlineListCallback<TbPtrList<CFSOnlineCity>>::Type callback, const void* context) {
    ev("GetList", {lab(this), lab(reinterpret_cast<const void*>(callback)), lab(context)});
}
void PostGetCityListCallbackFunction(int, const void*, TbPtrList<CFSOnlineCity>*) {}
void RemoveWorldLobbyController() { ev("RemoveWorldLobbyController", {}); }
void HideWorldLobbySats() { ev("HideWorldLobbySats", {}); }
void ShowWorldLobbySats() { ev("ShowWorldLobbySats", {}); }
CLBGlobeCityFlag::CLBGlobeCityFlag(const CLBScreenComponentOnlineGlobe* globe, const CFSOnlineCity* city) {
    ev("CityFlag", {newlab(this), lab(globe), static_cast<int64_t>(reinterpret_cast<uintptr_t>(city) & 0xffffffffu)});
}

typedef TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*> FlagList;

#pragma clang diagnostic ignored "-Winvalid-offsetof"
extern "C" {

uint64_t tpw_g_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"lobby", &gLobby}, {"view", &gLobbyView}, {"meshes", &gLobbyMeshes}, {"screens", &gLobbyScreens},
        {"sys", &gOnlineSystem}, {"thunder", &gLobbyThunderBank}, {"rain", &gei_Rain}, {"display", &gei_Display.mFlags},
        {"fakemesh", &gFakeMesh}, {"fakecitylist", &gFakeCityList},
        {"cb", reinterpret_cast<const void*>(&PostGetCityListCallbackFunction)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_g_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_g_newglobe() {
    void* memory = tpw::ArenaAllocZeroed(sizeof(TestGlobe));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (memory) TestGlobe()));
}

int32_t tpw_g_layout(int64_t* out) {
    int32_t n = 0;
    typedef CLBScreenComponentOnlineGlobe G;
    out[n++] = offsetof(G, mVisible);
    out[n++] = offsetof(G, mGlobeObj);
    out[n++] = offsetof(G, mF0C);
    out[n++] = offsetof(G, mF1C);
    out[n++] = offsetof(G, mF20);
    out[n++] = offsetof(G, mF24);
    out[n++] = offsetof(G, mF28);
    out[n++] = offsetof(G, mF2C);
    out[n++] = offsetof(G, mF30);
    out[n++] = offsetof(G, mF34);
    out[n++] = offsetof(G, mF38);
    out[n++] = offsetof(G, mF44);
    out[n++] = offsetof(G, mCityFlags);
    out[n++] = offsetof(G, mF74);
    out[n++] = offsetof(G, mF78);
    out[n++] = sizeof(TestGlobe);
    out[n++] = offsetof(CLBLobbyView, mFov);
    out[n++] = offsetof(CLBLobbyView, mLightUpOffset);
    out[n++] = offsetof(CLBLobbyView, mLightRightOffset);
    out[n++] = offsetof(CLBLobbyView, mLightDownward);
    out[n++] = offsetof(CLBLobbyView, mSkyTarget);
    out[n++] = sizeof(CLBLobbyView);
    out[n++] = offsetof(CLBLobby, mGlobals);
    out[n++] = sizeof(CLBLobby);
    out[n++] = offsetof(CLBLobbyGlobals, mGlobeRadiusOut);
    out[n++] = sizeof(CLBLobbyGlobals);
    out[n++] = offsetof(CLBScreenSystem, mAdvisor);
    out[n++] = sizeof(CLBScreenSystem);
    out[n++] = offsetof(CLBScreenComponentAdvisor, mHasTimeout);
    out[n++] = sizeof(CLBScreenComponentAdvisor);
    out[n++] = offsetof(CFSOnlineSystem, mCityList);
    out[n++] = sizeof(CFSOnlineSystem);
    out[n++] = offsetof(CLBGlobe, mMesh);
    out[n++] = offsetof(CLBGlobe, mScreen);
    out[n++] = sizeof(CLBGlobe);
    return n;
}

// Lists: kind 0 is a TbPtrList<CFSOnlineCity>, kind 1 the flag list.
uint64_t tpw_g_newlist(int32_t kind) {
    if (kind == 0) return reinterpret_cast<uintptr_t>(new TbPtrList<CFSOnlineCity>());
    return reinterpret_cast<uintptr_t>(new FlagList());
}
void tpw_g_add(int32_t kind, uint64_t list, uint64_t id) {
    if (kind == 0) reinterpret_cast<TbPtrList<CFSOnlineCity>*>(list)->AddTail(reinterpret_cast<CFSOnlineCity*>(id));
    else reinterpret_cast<FlagList*>(list)->AddTail(reinterpret_cast<CLBGlobeCityFlag*>(id));
}
// out: count, the tail's value, then the values from the head.
int32_t tpw_g_dumpflags(uint64_t list, int64_t* out) {
    FlagList* l = reinterpret_cast<FlagList*>(list);
    out[0] = l->mCount;
    out[1] = l->mTail ? static_cast<int64_t>(reinterpret_cast<uintptr_t>(l->mTail->mData)) : 0;
    int32_t n = 0;
    for (FlagList::CNode* node = l->mHead; node != nullptr && n < 30; node = node->mNext) out[2 + n++] = reinterpret_cast<uintptr_t>(node->mData);
    return n;
}

void tpw_g_show(uint64_t g) { reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->Show(); }
void tpw_g_hide(uint64_t g) { reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->Hide(); }
void tpw_g_zin(uint64_t g) { reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->GlobeZoomIn(); }
void tpw_g_zout(uint64_t g) { reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->GlobeZoomOut(); }
void tpw_g_sort(uint64_t g) { reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->SortOutZoomConstants(); }
void tpw_g_create(uint64_t g, uint64_t list) {
    reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->CreateCities(reinterpret_cast<TbPtrList<CFSOnlineCity>*>(list));
}
void tpw_g_unreg(uint64_t g, uint64_t id) {
    reinterpret_cast<CLBScreenComponentOnlineGlobe*>(g)->UnRegister(reinterpret_cast<CLBGlobeCityFlag*>(id));
}

}  // extern "C"
