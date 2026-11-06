// Test library for the online globes' list of globes (libtpw_unit_CLBGlobe_Register.dylib, tests/test_CLBGlobe_Register.py), main app:
//   include/lobby.h                   RegisterGlobe, UnRegisterGlobe (slots 0x38, 0x3c)
//   src/game/online/lobby_globe.cpp   Register, UnRegister, ~CLBGlobe, the three Hides, the three destructors, the constructor
//
// The screens are the real classes and every call goes through the host's vtable, so what runs is what the game's Show and ~CLBGlobe
// reach. Only the callees outside the list (the base class's destructor, the controllers, the meshes, the sound) are fakes.
#define TPW_GLOBE_SHOWHIDE_REAL   // as src/game/online/lobby_globe.cpp: the real Hide, not the inline default
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "FS_Online.h"
#include "TPSound.h"
#include "tb_resource_display.h"

using namespace calllog;

// ---- globals of other files, defined here
CDisplay gei_Display;
CLBLobbyMeshes* gLobbyMeshes;
CLBScreenSystem* gLobbyScreens;
CFSOnlineSystem* gOnlineSystem;
CDisplay::CDisplay() {}

// ---- the members the vtables name, which no case reaches
CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() { ev("baseDtor", {lab(this)}); }
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentOnlineGlobe::Show() {}
void CLBScreenComponentDownloadGlobe::Init() {}
void CLBScreenComponentDownloadGlobe::Tick() {}
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentDownloadGlobe::Show() {}
void CLBScreenComponentDownloadGlobe::Slot50() {}
void CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() {}
void CLBScreenComponentPublishGlobe::Init() {}
void CLBScreenComponentPublishGlobe::Tick() {}
void CLBScreenComponentPublishGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentPublishGlobe::Show() {}
void CLBScreenComponentPublishGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentPublishGlobe::HandleOnlineBacktrack() {}
CLBGlobeCityFlag::~CLBGlobeCityFlag() {}

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMesh", {lab(this)}); }
void RemoveWorldLobbyController() { ev("RemoveWorldLobbyController", {}); }
void RemoveOnlineCityInfoOverlay() { ev("RemoveOnlineCityInfoOverlay", {}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("Fade", {handle}); }
CFSOnlineSystem::CFSOnlineSystem() {}
void CFSOnlineSystem::SetPublishing(int32_t publishing, const char* text, const uint16_t* words) {
    ev("SetPublishing", {lab(this), publishing, text != nullptr, words != nullptr});
}

namespace {
// The Mac's base class leaves slot 0x0c pure; only this makes it a class one can make.
class TestOnlineGlobe : public CLBScreenComponentOnlineGlobe {
public:
    void Init() override {}
};
typedef CLBScreenComponentOnlineGlobe Screen;
uint8_t gMesh[16];
uint8_t gSystem[16];
}  // namespace

extern "C" {

uint64_t tpw_rg_addr(const char* name) {
    if (std::strcmp(name, "mesh") == 0) return reinterpret_cast<uintptr_t>(gMesh);
    if (std::strcmp(name, "sys") == 0) return reinterpret_cast<uintptr_t>(gSystem);
    return 0;
}

// cls: 0 the online globe, 1 the download globe, 2 the publish globe. The real constructors make the lists.
uint64_t tpw_rg_screen(int32_t cls, int32_t visible, int32_t f74, int32_t f78, uint32_t flags) {
    Screen* screen;
    if (cls == 0) screen = new TestOnlineGlobe;
    else if (cls == 1) screen = new CLBScreenComponentDownloadGlobe;
    else screen = new CLBScreenComponentPublishGlobe;
    screen->mVisible = visible;
    screen->mF74 = f74;
    screen->mF78 = f78;
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(gMesh);
    gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(gSystem);
    gei_Display.mFlags = flags;
    return reinterpret_cast<uintptr_t>(screen);
}

uint64_t tpw_rg_globe(uint64_t screen) {
    CLBGlobe* globe = new CLBGlobe;
    globe->mMesh = nullptr;
    globe->mScreen = reinterpret_cast<Screen*>(screen);
    return reinterpret_cast<uintptr_t>(globe);
}

void tpw_rg_reg(uint64_t screen, uint64_t globe) { reinterpret_cast<Screen*>(screen)->RegisterGlobe(reinterpret_cast<CLBGlobe*>(globe)); }
void tpw_rg_unreg(uint64_t screen, uint64_t globe) { reinterpret_cast<Screen*>(screen)->UnRegisterGlobe(reinterpret_cast<CLBGlobe*>(globe)); }
void tpw_rg_del(uint64_t globe) { delete reinterpret_cast<CLBGlobe*>(globe); }
void tpw_rg_clear(uint64_t screen) { reinterpret_cast<Screen*>(screen)->DeleteGlobeListEntries(); }
void tpw_rg_hide(uint64_t screen) { reinterpret_cast<Screen*>(screen)->Hide(); }
void tpw_rg_destroy(uint64_t screen) { delete reinterpret_cast<Screen*>(screen); }
void tpw_rg_visible(uint64_t screen, int32_t value) { reinterpret_cast<Screen*>(screen)->mVisible = value; }

// out: visible, f74, f78, the display's flags, the list's count, the entries head to tail, -1, the entries tail to head; answers how many.
int32_t tpw_rg_read(uint64_t screen, int32_t* out) {
    Screen* s = reinterpret_cast<Screen*>(screen);
    int32_t n = 0;
    out[n++] = s->mVisible;
    out[n++] = s->mF74;
    out[n++] = s->mF78;
    out[n++] = static_cast<int32_t>(gei_Display.mFlags);
    out[n++] = s->mGlobes->mCount;
    for (auto* node = s->mGlobes->mHead; node != nullptr; node = node->mNext) out[n++] = lab(node->mData);
    out[n++] = -1;
    for (auto* node = s->mGlobes->mTail; node != nullptr; node = node->mPrev) out[n++] = lab(node->mData);
    return n;
}

}  // extern "C"
