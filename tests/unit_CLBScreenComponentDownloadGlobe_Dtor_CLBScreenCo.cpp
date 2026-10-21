// Test library for the online download globe's destructor and Hide (unit v5553; libtpw_unit_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.dylib,
// tests/test_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.py), main app:
//   src/game/online/lobby_globe.cpp   ~CLBScreenComponentDownloadGlobe, Hide
//
// The callees outside the unit are recording fakes: the base classes' constructors and destructors, IsVisible, the globes' and flags'
// destructors, operator delete (it frees nothing: the world's blocks are static or leaked), the controllers, the meshes, the sound.
// The globals of other files (gei_Display, gLobbyMeshes) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "tb_resource_display.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CDisplay gei_Display;
CLBLobbyMeshes* gLobbyMeshes;

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
CDisplay::CDisplay() {}

CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() { ev("baseDtor", {lab(this)}); }
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() {
    ev("IsVisible", {lab(this)});
    return static_cast<int32_t>(ret("IsVisible"));
}

// The online globe's own members are not part of this unit: its destructor is empty here (the Mac's download globe does not call it).
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}

void CLBScreenComponentDownloadGlobe::Init() {}
void CLBScreenComponentDownloadGlobe::Tick() {}
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentDownloadGlobe::Show() {}

CLBGlobe::~CLBGlobe() { ev("globeDtor", {lab(this)}); }
CLBGlobeCityFlag::~CLBGlobeCityFlag() { ev("flagDtor", {lab(this)}); }

void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMesh", {lab(this)}); }
void RemoveWorldLobbyController() { ev("RemoveWorldLobbyController", {}); }
void RemoveOnlineCityInfoOverlay() { ev("RemoveOnlineCityInfoOverlay", {}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("Fade", {handle}); }

// operator delete frees nothing and records the block (a node of a list shows as label -1).

namespace {
// The world's blocks come from the arena afresh for every case (the program's operator delete may free them).
uint8_t* gThisBuf;
uint8_t* gGlistBuf;
uint8_t* gFlistBuf;
uint8_t* gMeshBuf;
uint8_t* gGlobeBuf[6];
uint8_t* gFlagBuf[6];
CLBScreenComponentDownloadGlobe* gThis;
int32_t gSnapshot[5];
bool gSnapshotValid;
uint8_t* Blk(std::size_t n) {
    uint8_t* block = static_cast<uint8_t*>(::operator new(n < 16 ? 16 : n));
    std::memset(block, 0, n < 16 ? 16 : n);
    return block;
}
}  // namespace

extern "C" {

uint64_t tpw_dg_addr(const char* name) {
    if (std::strcmp(name, "this") == 0) return reinterpret_cast<uintptr_t>(gThisBuf);
    if (std::strcmp(name, "glist") == 0) return reinterpret_cast<uintptr_t>(gGlistBuf);
    if (std::strcmp(name, "flist") == 0) return reinterpret_cast<uintptr_t>(gFlistBuf);
    if (std::strcmp(name, "mesh") == 0) return reinterpret_cast<uintptr_t>(gMeshBuf);
    if (std::strcmp(name, "flags") == 0) return reinterpret_cast<uintptr_t>(&gei_Display.mFlags);
    if (name[0] == 'g' && name[1] >= '0' && name[1] <= '5') return reinterpret_cast<uintptr_t>(gGlobeBuf[name[1] - '0']);
    if (name[0] == 'f' && name[1] >= '0' && name[1] <= '5') return reinterpret_cast<uintptr_t>(gFlagBuf[name[1] - '0']);
    return 0;
}

uint64_t tpw_dg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dg_layout(int64_t*) { return 0; }

// Builds the object: the lists hold the first `gcount` globe slots (bit i of gmask: an object, else NULL), the same for the flags.
void tpw_dg_build(int32_t vis, int32_t f74, int32_t f78, uint32_t gmask, int32_t gcount, uint32_t fmask, int32_t fcount, uint32_t flags) {
    gSnapshotValid = false;
    gThisBuf = Blk(sizeof(CLBScreenComponentDownloadGlobe));
    gGlistBuf = Blk(sizeof(TbPtrList<CLBGlobe>));
    gFlistBuf = Blk(sizeof(TbPtrList<CLBGlobeCityFlag>));
    gMeshBuf = Blk(64);
    for (int i = 0; i < 6; ++i) {
        gGlobeBuf[i] = Blk(16);
        gFlagBuf[i] = Blk(16);
    }
    gThis = new (gThisBuf) CLBScreenComponentDownloadGlobe;
    gThis->mVisible = vis;
    gThis->mF74 = f74;
    gThis->mF78 = f78;
    gThis->mGlobes = new (gGlistBuf) TbPtrList<CLBGlobe>;
    gThis->mCityFlags = new (gFlistBuf) TbPtrList<CLBGlobeCityFlag>;
    for (int32_t i = 0; i < gcount; ++i)
        gThis->mGlobes->AddTail(((gmask >> i) & 1) ? new (gGlobeBuf[i]) CLBGlobe : nullptr);
    for (int32_t i = 0; i < fcount; ++i)
        gThis->mCityFlags->AddTail(((fmask >> i) & 1) ? new (gFlagBuf[i]) CLBGlobeCityFlag : nullptr);
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(gMeshBuf);
    gei_Display.mFlags = flags;
}

// vis, globes list present, city flags list present, f74, f78
void tpw_dg_read(int32_t* out) {
    if (gSnapshotValid) {
        std::memcpy(out, gSnapshot, sizeof(gSnapshot));
        return;
    }
    out[0] = gThis->mVisible;
    out[1] = gThis->mGlobes != nullptr;
    out[2] = gThis->mCityFlags != nullptr;
    out[3] = gThis->mF74;
    out[4] = gThis->mF78;
}

void tpw_dg_hide() { gThis->Hide(); }

// The destructor with the hidden flag (a signed 16 bit value); answers this (the Mac answers r3), 0 for a NULL object.
uint64_t tpw_dg_dtor(int32_t flag, int32_t isNull) {
    if (isNull) return 0;
    // The deleting form is the destructor, then operator delete of the block; the fields are read between the two (the block is freed after).
    gThis->~CLBScreenComponentDownloadGlobe();
    gSnapshot[0] = gThis->mVisible;
    gSnapshot[1] = gThis->mGlobes != nullptr;
    gSnapshot[2] = gThis->mCityFlags != nullptr;
    gSnapshot[3] = gThis->mF74;
    gSnapshot[4] = gThis->mF78;
    gSnapshotValid = true;
    if (static_cast<int16_t>(flag) > 0) ::operator delete(gThisBuf);
    return reinterpret_cast<uintptr_t>(gThisBuf);
}

}  // extern "C"
