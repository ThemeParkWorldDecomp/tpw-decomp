// Test library for the online islands screen's CreateParks, Show and ProcessCancel (unit v5639;
// libtpw_unit_CLBScreenComponentOnlineIslands_CreateParks.dylib, tests/test_CLBScreenComponentOnlineIslands_CreateParks.py), main app:
//   src/CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_CreateParks.cpp
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. The strings'
// members are fakes too (the text of a string is its hash in the log). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "lobby.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
CLBLobbyMeshes* gLobbyMeshes;
CLBLobbyView* gLobbyView;
CLBLobby* gLobby;
OnlineParkSelection* gOnlineParkSelection;
int32_t* gLobbyIslandSounds;
// gGame is only ever used as the address the fake gets (its constructor is inline in a file this library does not compile).
alignas(16) unsigned char gGameBytes[sizeof(CGame)] __asm__("_gGame");

namespace {

CFSOnlineSystemThemeList* gNewThemeList;
CFSOnlineSystemParkList_InCityTop10* gNewTop10;
int32_t gNewF384;

template <class Char>
int64_t TextHash(const Char* text) {
    uint32_t h = 0;
    if (text != nullptr)
        for (; *text != 0; ++text) h = h * 31u + static_cast<uint32_t>(static_cast<std::make_unsigned_t<Char>>(*text));
    return h;
}

// Text the fakes hand out: a ring of buffers.
char16_t* WidePool(const char* a, const char* b) {
    static char16_t pool[256][96];
    static int next = 0;
    char16_t* out = pool[next++ & 255];
    int n = 0;
    for (const char* p = a; *p != 0 && n < 90; ++p) out[n++] = static_cast<unsigned char>(*p);
    for (const char* p = b; *p != 0 && n < 90; ++p) out[n++] = static_cast<unsigned char>(*p);
    out[n] = 0;
    return out;
}

}  // namespace

// ---- the string class: constructors, destructor and virtual members are fakes (imports on the Mac)
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    ev(sizeof(Char) == 1 ? "StrCtorC0" : "StrCtorW", {newlab(this)});
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char* text) {
    mLength = 0;
    mData = const_cast<Char*>(text);
    mCapacity = 0;
    mGrowBy = 0;
    ev("StrCtorC", {newlab(this)});
    ev("StrText", {TextHash(text)});
}
template <class Char>
TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {
    ev(sizeof(Char) == 1 ? "StrDtorC" : "StrDtorW", {lab(this)});
}
template <class Char>
void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template <class Char>
void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template <class Char>
void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char*) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>&) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) { return *this; }
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) { return *this; }

template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;

// ---- the fakes
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    ev("StringToUniString", {lab(&out), lab(&in)});
    out.mData = WidePool(in.mData, "");
    out.mLength = 0;
    return 0;
}

void CGame::GetLocalisedThemeGateNames(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& gate1,
                                       TbDynamicStringTemplate<char16_t>& gate2) const {
    ev("GetLocalisedThemeGateNames", {lab(this), lab(&theme), lab(&gate1), lab(&gate2), TextHash(theme.mData)});
    char narrowText[96];
    int n = 0;
    for (const char16_t* p = theme.mData; p != nullptr && *p != 0 && n < 90; ++p) narrowText[n++] = static_cast<char>(*p);
    narrowText[n] = 0;
    gate1.mData = WidePool(narrowText, "g1");
    gate2.mData = WidePool(narrowText, "g2");
}

char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    ev("ReturnOnlineThemeName", {lab(this), theme});
    static char pool[64][32];
    static int next = 0;
    char* out = pool[next++ & 63];
    std::snprintf(out, 32, "T%d", theme);
    return out;
}

void CLBScreenComponentIslands::ParseThemeDescriptor(const char* theme, const CFSOnlinePark* park, int32_t order, const char16_t* gate1,
                                                     const char16_t* gate2) {
    ev("ParseThemeDescriptor", {lab(this), lab(park), order, TextHash(theme), TextHash(gate1), TextHash(gate2)});
}

void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }

CFSOnlineSystem* mainmenu_callee::NewOnlineSystem() {
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(0x2000));
    ev("NewOnlineSystem", {newlab(system)});
    system->mThemeList = gNewThemeList;
    system->mParksTop10 = gNewTop10;
    system->mF384 = gNewF384;
    return system;
}

int32_t mainmenu_callee::ScreenIsVisible(void* screen) {
    ev("IsVisible", {lab(screen)});
    return static_cast<int32_t>(ret("IsVisible"));
}

void parkinfo_callee::OnlineIslandsSlot30(void* screen) { ev("Slot30", {lab(screen)}); }

void CFSOnlineSystemParkList_InCityTop10::GetList(int32_t cityID, Callback callback, const void* context) {
    ev("GetList", {lab(this), cityID, callback == PostGetTop10ParksListCallbackFunction ? 1 : 0, lab(context)});
}

void PostGetTop10ParksListCallbackFunction(int32_t, const void*, TbPtrList<CFSOnlinePark>*) {}

void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) { ev("PlayAnim", {lab(this), anim, loop}); }
void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }
void CreateCityLobbyController() { ev("CreateCityLobbyController", {}); }
void ShowCityLobbyController() { ev("ShowCityLobbyController", {}); }
void CLBLobbyMeshes::ShowHeightfield() { ev("ShowHeightfield", {lab(this)}); }
void CLBLobbyView::SetGlobalAmbientLight(int32_t enabled, uint32_t color) { ev("SetGlobalAmbientLight", {lab(this), enabled, color}); }
void CLBLobbyView::SetGlobalDirectionalLight(int32_t enabled, const uint32_t* color, const s3DPoint* normal, int32_t followsView) {
    ev("SetGlobalDirectionalLight", {lab(this), enabled, color != nullptr ? *color : 0u, normal != nullptr ? 1 : 0, followsView});
}
void CLBLobbyView::SetFogOn() { ev("SetFogOn", {lab(this)}); }
void CLBLobbyView::SetFogSettings(uint32_t color, float nearDistance, float farDistance) {
    ev("SetFogSettings", {lab(this), color, fbits(nearDistance), fbits(farDistance)});
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}
void SOUND_Tick() { ev("SOUND_Tick", {}); }

extern "C" {

uint64_t tpw_oi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
        {"gLobbyScreens", &gLobbyScreens},
        {"gLobbyMeshes", &gLobbyMeshes},
        {"gLobbyView", &gLobbyView},
        {"gLobby", &gLobby},
        {"gOnlineParkSelection", &gOnlineParkSelection},
        {"gLobbyIslandSounds", &gLobbyIslandSounds},
        {"gGame", gGameBytes},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_oi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// What NewOnlineSystem (the fake of the allocation and the constructor) puts into the system it makes.
void tpw_oi_prepare(uint64_t themeList, uint64_t top10, int32_t f384) {
    gNewThemeList = reinterpret_cast<CFSOnlineSystemThemeList*>(static_cast<uintptr_t>(themeList));
    gNewTop10 = reinterpret_cast<CFSOnlineSystemParkList_InCityTop10*>(static_cast<uintptr_t>(top10));
    gNewF384 = f384;
}

// The host offsets the module writes by (the order of NAMES in the module).
int32_t tpw_oi_layout(int64_t* out) {
    int32_t n = 0;
    using S = CLBScreenComponentOnlineIslands;
    out[n++] = static_cast<int64_t>(offsetof(S, mVisible));
    out[n++] = static_cast<int64_t>(offsetof(S, mIslands));
    out[n++] = static_cast<int64_t>(offsetof(S, mCurrent));
    out[n++] = static_cast<int64_t>(offsetof(S, mLast));
    out[n++] = static_cast<int64_t>(offsetof(S, mCameraState));
    out[n++] = static_cast<int64_t>(offsetof(S, mFE4));
    out[n++] = static_cast<int64_t>(offsetof(TbPtrList<CFSOnlinePark>, mHead));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mMeshB));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mSoundSet));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mSound1));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mSound2));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlinePark, mF4C));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mParksTop10));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mThemeList));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineSystem, mF384));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mAdvisor));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobbyView, mFov));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobbyView, mLightUpOffset));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobbyView, mLightRightOffset));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobbyView, mLightDownward));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mGlobals));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobbyGlobals, mIslandFov));
    out[n++] = static_cast<int64_t>(offsetof(OnlineParkSelection, mID));
    // the sizes: the module allocates at least this much
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineSystem));
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlinePark));
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenComponentOnlineIslands));
    return n;
}

// ---- the functions, one wrapper each. The screen is raw memory (its constructors are not compiled here): the calls are qualified,
// so no vtable is read.
void tpw_oi_create_parks(uint64_t screen, uint64_t parks) {
    auto* s = reinterpret_cast<CLBScreenComponentOnlineIslands*>(static_cast<uintptr_t>(screen));
    s->CLBScreenComponentOnlineIslands::CreateParks(reinterpret_cast<TbPtrList<CFSOnlinePark>*>(static_cast<uintptr_t>(parks)));
}
void tpw_oi_show(uint64_t screen) {
    reinterpret_cast<CLBScreenComponentOnlineIslands*>(static_cast<uintptr_t>(screen))->CLBScreenComponentOnlineIslands::Show();
}
uint32_t tpw_oi_process_cancel(uint64_t screen) {
    return reinterpret_cast<CLBScreenComponentOnlineIslands*>(static_cast<uintptr_t>(screen))
        ->CLBScreenComponentOnlineIslands::ProcessCancel();
}

}  // extern "C"
