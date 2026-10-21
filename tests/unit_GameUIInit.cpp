// Unit w116: all set-up and teardown functions run for real; external callees record every argument.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "Mouse.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "FS_Online.h"
#include "Light.h"
#include "Text.h"
#include "TPSettings.h"
#include "tb_resource_system.h"
#include "Debug.h"
#include "calllog.h"

namespace {
void* gAllocated[64];
int32_t gAllocatedCount;
bool KeepBlock(void* p, bool);
}
#define TPW_RELEASE_OBSERVER KeepBlock
#include "calllog.cpp"
using namespace calllog;

InterfaceWindow* gUIRootWindow;
InterfaceWindow* gUIPointerIn;
InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gUIPanelWindow;
InterfaceWindow* gCoasterPanel;
CText gText;
CPlayerSettings gPlayerSettings;
UIInterface gUIInterface;
CLight gUILight;
CSystem gei_System;
namespace { int32_t gMapStore; }
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(&gMapStore);

// Constructors required by globals and their by-value members are silent.
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CSystem::CSystem() {}
CMapLinkBase::~CMapLinkBase() {}
CLight::CLight() {}
CLight::~CLight() {}
int32_t CLight::GetLinkType() { return 1; }
void* CLight::GetData() { return nullptr; }
int32_t CLight::Prepare() { return 1; }
CGameType::CGameType() { mType = kGameTypeNormal; }

namespace {
int32_t gReleaseTo = -1;
void* Obj(int32_t label);
class FakeMode : public CMouseMode {
public:
    ~FakeMode() override { ev("Mode.dtor", {lab(this)}); }
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode", 0));
    }
    void OnAssign() override { ev("OnAssign", {lab(this), lab(gMouseMode)}); }
    void OnRelease() override {
        ev("OnRelease", {lab(this), lab(gMouseMode)});
        if (gReleaseTo >= 0) gMouseMode = static_cast<CMouseMode*>(Obj(gReleaseTo));
    }
};
alignas(16) unsigned char gWindows[24][sizeof(InterfaceWindow)];
alignas(16) unsigned char gModes[3][sizeof(FakeMode)];
alignas(16) unsigned char gDefault[sizeof(CMMDefault)];
alignas(16) unsigned char gMouseStore[sizeof(CMouse)];
unsigned char gDrawStore[16];

bool KeepBlock(void* p, bool) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] == p) return true;
    }
    for (const auto& store : gWindows) {
        if (p == store) return true;
    }
    for (const auto& store : gModes) {
        if (p == store) return true;
    }
    return p == gDefault;
}

void* Obj(int32_t label) {
    if (label >= 1 && label <= 24) return gWindows[label - 1];
    if (label >= 26 && label <= 28) return gModes[label - 26];
    if (label >= 40 && label <= 49) return gStaticTags[label - 40].mPayload;
    switch (label) {
        case 25: return gMouseStore;
        case 29: return gDefault;
        case 30: return &gText.mFonts[6];
        case 31: return gDrawStore;
        case 32: return &gei_System;
        case 33: return gei_MapWho;
        case 34: return static_cast<CMapLinkBase*>(&gUILight);
        case 35: return &gText;
        case 36: return &gUIInterface;
    }
    return nullptr;
}

// Native global addresses in exactly the Python state order; pointer values travel as labels.
void* PointerGlobal(int i) {
    switch (i) {
        case 0: return &gUIRootWindow;
        case 1: return &gUIPointerIn;
        case 2: return &gGameWindow;
        case 3: return &gFirstPersonWindow;
        case 4: return &gUIPanelWindow;
        case 5: return &gCashPanel;
        case 6: return &gCoasterPanel;
        case 7: return &gControlPanel;
        case 8: return &gTagPanel;
        case 9: return &gTagPanelSource;
        case 10: return &gStaffPanelA;
        case 11: return &gStaffPanelB;
        case 12: return &gStaffPanelC;
        case 13: return &gMouse;
        case 14: return &gMouseMode;
        case 15: return &gUISelectedThing;
        case 16: return &gUISelectedPeep;
        case 17: return &gUISelectedObject;
    }
    return nullptr;
}
uint32_t FlagHash() {
    uint32_t hash = 2166136261u;
    for (const auto& tag : gStaticTags) hash = (hash ^ static_cast<uint16_t>(tag.mActive)) * 16777619u;
    return hash;
}
int Script(int16_t* script) {
    if (script == gFirstPersonWindowScript) return 1;
    if (script == gTagPanelScript) return 2;
    if (script == gCashPanelScript) return 3;
    if (script == gCoasterPanelScript) return 4;
    if (script == gControlPanelScript) return 5;
    return 0;
}
int Callback(InterfaceWindowCallback callback) {
    if (callback == Engine_GameWinMsg) return 1;
    if (callback == Engine_GameFirstPersonMsg) return 2;
    if (callback == TagCallback) return 3;
    if (callback == TagDlgCallback) return 4;
    if (callback == CashPanelMsg) return 5;
    if (callback == CoasterPanelMsg) return 6;
    if (callback == ControlPanelMsg) return 7;
    return 0;
}
void LightEvent(const char* name) {
    ev(name, {lab(&gei_System), gUILight.mColorWord, fbits(gUILight.mColor[0]), fbits(gUILight.mColor[1]),
              fbits(gUILight.mColor[2]), fbits(gUILight.mColor[3]), fbits(gUILight.mNormal.x), fbits(gUILight.mNormal.y),
              fbits(gUILight.mNormal.z)});
}
}

// Nothrow allocation uses a failure queue, preserves the fill, and records the Mac size and pre-allocation tag flags.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = size;
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault) || size == sizeof(CMMOnline)) mac = 8;
    else if (size == sizeof(InterfaceWindow)) mac = 0x138;
    else if (size == sizeof(InterfaceTextObjectSprite)) mac = 0x19c;
    void* block = nullptr;
    if (ret("New", 1)) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        gAllocated[gAllocatedCount++] = block;
        newlab(block);
        ++tpw_cl_heap[0];
    }
    ev("operator_new", {mac, lab(block), FlagHash()});
    return block;
}

// The unit links no arena. The windows take the game heap (include/InterfaceWindow.h, unit v5L31), which is the arena on the host;
// here it is the recording nothrow new above, and a block goes back to the global operator delete.
namespace tpw {
void* ArenaAlloc(std::size_t size) { return ::operator new(size, std::nothrow); }
bool InArena(const void*) { return false; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

// Online mode remains a fake. The real default mode's OnAssign calls two recording external leaves.
CMMOnline::CMMOnline() { ev("Online.ctor", {lab(this)}); }
CMMOnline::~CMMOnline() { ev("Online.dtor", {lab(this)}); }
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { ev("Online.GetMode", {lab(this)}); return 7; }
void CMMOnline::OnAssign() { ev("Online.OnAssign", {lab(this), lab(gMouseMode)}); }
void UIInterface::CheckMouseContext(int context) { ev("CheckMouseContext", {lab(this), context}); }
void CTRACE(uint32_t level, const char* text, ...) { ev("CTRACE", {level}, {text}); }

void UI_FlushAllMessages() { ev("Flush"); }
void CText::InitThemeSpecific() { ev("Text.InitTheme", {lab(this)}); }
void CText::DeInitThemeSpecific() { ev("Text.DeInitTheme", {lab(this)}); }
void CreateMainPanel() { ev("CreateMainPanel"); }
void InitStaffPanel() { ev("InitStaffPanel"); }
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {lab(gUIPanelWindow)}); }
void UI_EnableHelpText() { ev("EnableHelpText"); }
void UI_DisableHelpText() { ev("DisableHelpText"); }
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(w), m, a, b});
    return 0;
}
void UI_SetCursor(int16_t cursor) { ev("UI_SetCursor", {cursor}); }
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    InterfaceWindow** panels[] = {&gCashPanel, &gCoasterPanel, &gControlPanel, &gTagPanel};
    const char* names[] = {"SwapCash", "SwapCoaster", "SwapControl", "SwapTag"};
    for (int i = 0; i < 4; ++i) {
        const int32_t to = ret(names[i], -1);
        if (to >= 0) *panels[i] = static_cast<InterfaceWindow*>(Obj(to));
    }
    return 1;
}
void CSystem::UpdateLights() { LightEvent("UpdateLights"); }
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) { ev("Link", {lab(this), lab(link), list}); return 1; }
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) { ev("Unlink", {lab(this), lab(link), list}); return 1; }

// Only callback identities are compared; they are never invoked by this unit.
#define CALLBACK(name) int32_t name(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
CALLBACK(Engine_GameWinMsg)
CALLBACK(Engine_GameFirstPersonMsg)
CALLBACK(TagCallback)
CALLBACK(TagDlgCallback)
CALLBACK(CashPanelMsg)
CALLBACK(CoasterPanelMsg)
CALLBACK(ControlPanelMsg)

// Window leaves record all arguments. Constructor initializes only the fields our functions read.
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b) {
    mNext = nullptr;
    mPrevious = nullptr;
    mChild = nullptr;
    mParent = parent;
    ev("Window.ctor", {lab(this), lab(parent), attributes, id, l, t, r, b});
}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attr, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b) {
    ev("CreateObject", {lab(this), kind, attr, id, l, t, r, b});
    return static_cast<InterfaceWindow*>(Obj(ret("Create", 0)));
}
InterfaceWindow* InterfaceWindow::Build(int16_t* script, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), Script(script), Callback(callback)});
    return static_cast<InterfaceWindow*>(Obj(ret("Build", 5)));
}
int32_t InterfaceWindow::Show(int16_t on) { ev("Show", {lab(this), on}); return 1; }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) { ev("SetCallback", {lab(this), Callback(callback)}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(ret("Child", 0)));
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = ret("RectL", 0);
    rect->mTop = ret("RectT", 0);
    rect->mRight = ret("RectR", 0);
    rect->mBottom = ret("RectB", 0);
    return 1;
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    const int64_t param = index == 2 ? lab(value) : reinterpret_cast<intptr_t>(value);
    ev("setExtraParam", {lab(this), index, param});
    return 1;
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
InterfaceFont* GetFontTableEntry(uint32_t i) { ev("GetFontTableEntry", {i}); return static_cast<InterfaceFont*>(Obj(30)); }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* text) {
    ev("FindDrawObject", {}, {text});
    return reinterpret_cast<InterfaceDrawObject*>(Obj(ret("Draw", 31)));
}
extern "C" void tpw_gi_sprite_ctor(InterfaceTextObjectSprite* self, void* data) __asm__(
#ifdef __APPLE__
    "__ZN25InterfaceTextObjectSpriteC1EPv"
#else
    "_ZN25InterfaceTextObjectSpriteC1EPv"
#endif
);
extern "C" void tpw_gi_sprite_ctor(InterfaceTextObjectSprite* self, void* data) { ev("TextSprite.ctor", {lab(self), lab(data)}); }

extern "C" {
void* tpw_gi_obj(int32_t label) { return Obj(label); }
// Free retained allocations at the beginning of the next case, never while their stale globals are inspected.
void tpw_gi_reset() {
    tpw_cl_on = 0;
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    for (auto& store : gWindows) {
        std::memset(store, 0xa5, sizeof store);
        new (store) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    }
    for (auto& store : gModes) new (store) FakeMode;
    std::memset(gDefault, 0xa5, sizeof gDefault);
}
void tpw_gi_state(const int32_t* in) {
    for (int i = 0; i < 18; ++i) {
        void* p = Obj(in[i]);
        std::memcpy(PointerGlobal(i), &p, sizeof p);
    }
    gMouseWord1E0 = in[18];
    gMouseWord1E4 = in[19];
    gUIRealScreenWidth = in[20];
    gUIRealScreenHeight = in[21];
    gPlayerSettings.mCurrent.mTooltipsOn = in[22] != 0;
    TheGameType().mType = static_cast<GameType>(in[23]);
    gReleaseTo = in[24];
    float* floats[] = {&gGameWindowCentreX, &gGameWindowCentreY, &gGameWindowHalfWidth, &gGameWindowHalfHeight,
                       &gUILight.mColor[0], &gUILight.mColor[1], &gUILight.mColor[2], &gUILight.mColor[3],
                       &gUILight.mNormal.x, &gUILight.mNormal.y, &gUILight.mNormal.z};
    for (int i = 0; i < 11; ++i) std::memcpy(floats[i], in + 25 + i, 4);
    gUILight.mColorWord = in[36];
    int k = 38;
    for (int i = 0; i < 10; ++i) {
        gStaticTags[i].mWindow = static_cast<InterfaceWindow*>(Obj(in[k++]));
        gStaticTags[i].mActive = in[k++];
        for (int j = 0; j < 0x202; ++j) gStaticTags[i].mPayload[j] = (in[37] + i * 7 + j) & 255;
    }
    for (int i = 0; i < 24; ++i) {
        auto* w = static_cast<InterfaceWindow*>(Obj(i + 1));
        w->mCursor = in[k++];
        w->mHelpText = in[k++];
        w->mNext = static_cast<InterfaceWindow*>(Obj(in[k++]));
        w->mPrevious = static_cast<InterfaceWindow*>(Obj(in[k++]));
        w->mChild = static_cast<InterfaceWindow*>(Obj(in[k++]));
        w->mParent = static_cast<InterfaceWindow*>(Obj(in[k++]));
    }
    for (int32_t& count : tpw_cl_heap) count = 0;
}
// Dump pointers by current label, float bits, every tag byte, and all touched fields of fixed and newly created windows.
int32_t tpw_gi_dump(int32_t* out) {
    int k = 0;
    for (int i = 0; i < 18; ++i) {
        void* p;
        std::memcpy(&p, PointerGlobal(i), sizeof p);
        out[k++] = lab(p);
    }
    out[k++] = gMouseWord1E0;
    out[k++] = gMouseWord1E4;
    out[k++] = gUIRealScreenWidth;
    out[k++] = gUIRealScreenHeight;
    out[k++] = gPlayerSettings.mCurrent.mTooltipsOn != 0;
    out[k++] = TheGameType().mType;
    out[k++] = gReleaseTo;
    float floats[] = {gGameWindowCentreX, gGameWindowCentreY, gGameWindowHalfWidth, gGameWindowHalfHeight,
                      gUILight.mColor[0], gUILight.mColor[1], gUILight.mColor[2], gUILight.mColor[3],
                      gUILight.mNormal.x, gUILight.mNormal.y, gUILight.mNormal.z};
    for (float f : floats) out[k++] = fbits(f);
    out[k++] = gUILight.mColorWord;
    for (const auto& tag : gStaticTags) {
        out[k++] = lab(tag.mWindow);
        out[k++] = tag.mActive;
        for (uint8_t b : tag.mPayload) out[k++] = b;
        if (tag.mWindow != nullptr) {
            out[k++] = tag.mWindow->mCursor;
            out[k++] = tag.mWindow->mHelpText;
        } else {
            out[k++] = 0;
            out[k++] = 0;
        }
    }
    for (int i = 0; i < 24; ++i) {
        auto* w = static_cast<InterfaceWindow*>(Obj(i + 1));
        out[k++] = w->mCursor;
        out[k++] = w->mHelpText;
        out[k++] = lab(w->mNext);
        out[k++] = lab(w->mPrevious);
        out[k++] = lab(w->mChild);
        out[k++] = lab(w->mParent);
    }
    out[k++] = reinterpret_cast<CMMDefault*>(gDefault)->mWord4;
    out[k++] = gMouseMode != nullptr ? gMouseMode->mWord4 : 0;
    for (int32_t count : tpw_cl_heap) out[k++] = count;
    return k;
}
int32_t tpw_gi_call(int32_t op, int32_t a, int32_t b) {
    switch (op) {
        case 0: gUIInterface.InitGame(); break;
        case 1: InitGameUI(); break;
        case 2: DeInitGameUI(); break;
        case 3: UI_ClearSelected(); break;
        case 4: reinterpret_cast<CMouse*>(Obj(25))->AssignMouseMode(static_cast<CMouseMode*>(Obj(a))); break;
        case 5: new (gDefault) CMMDefault; break;
        case 6: InitTagPanel(); break;
        case 7: CreateStaticTags(); break;
        case 8: InitCashPanel(); break;
        case 9: InitCoasterPanel(); break;
        case 10: InitControlPanel(); break;
        case 11: static_cast<InterfaceWindow*>(Obj(a))->setCursor(b); break;
        case 12: DeInitTagPanel(); break;
        case 13: DestroyStaticTags(); break;
        case 14: InstantRemoveAllTags(); break;
        case 15: DeInitCashPanel(); break;
        case 16: DeInitCoasterPanel(); break;
        case 17: DeInitControlPanel(); break;
        case 18: DeInitStaffPanel(); break;
        case 19: {
            auto* mode = new (gDefault) CMMDefault;
            if (static_cast<int16_t>(b) > 0) delete mode;
            else mode->~CMMDefault();
            break;
        }
        case 20: return reinterpret_cast<CMMDefault*>(gDefault)->CMMDefault::GetMode();
        case 21: reinterpret_cast<CMMDefault*>(gDefault)->CMMDefault::OnLeftClick(a, b); break;
        case 22: reinterpret_cast<CMMDefault*>(gDefault)->CMMDefault::OnRightClick(a, b); break;
        case 23: static_cast<InterfaceWindow*>(Obj(a))->unlink(); break;
        case 24: gUIInterface.DeInitGame(); break;
        case 25: reinterpret_cast<CMMDefault*>(gDefault)->CMMDefault::OnAssign(); break;
    }
    if (op == 5 || op == 19) return reinterpret_cast<CMMDefault*>(gDefault)->mWord4;
    return 0;
}
int16_t* tpw_gi_script(int32_t which) {
    int16_t* scripts[] = {gFirstPersonWindowScript, gTagPanelScript, gCashPanelScript, gCoasterPanelScript, gControlPanelScript};
    return scripts[which];
}
}
