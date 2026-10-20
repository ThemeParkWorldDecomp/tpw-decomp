// Fakes and test entry points for unit w101, the UI main panel helpers (libtpw_unit_UIMainPanel.dylib, driven by tests/test_UIMainPanel.py;
// see calllog.h):
//   src/ui_main_panel.cpp  UIMain_UISound 0x101802cc, UIMain_CheckPanel 0x10180514, UIMain_SetPanel 0x101805a0, UIMain_SetTitleText 0x10180794,
//                          UIMain_AdvisorMessage 0x101821cc, UIMain_CancelAdvisorMessage 0x1018227c, UIMain_DefaultPanelMsg 0x1018493c,
//                          UIMain_GetFullyOutOfFirstPerson 0x10187434
// Real: CoolRect (src/UI_Base.cpp). Every other callee is a fake that records the same event as the PowerPC side's stub: the sound call, the
// message sender, the keyboard capture decision, the key tables, the mouse and its default mode, the nothrow operator new (its answer comes from
// the queue "New"), the title text object's constructor, every InterfaceWindow member the title code calls, the advisor and the first-person
// functions. The objects the cases need have fixed labels (tpw_up_obj below); objects the code makes get labels as they are allocated. The
// events that read gUIPanelWindow record its label, which shows the order of the global's stores.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "calllog.h"
#include "keyboard.h"
#include "ui_main.h"

using namespace calllog;

// ---- the globals the code reads and writes (the Mac addresses are in the Python module)
InterfaceWindow* gUIPanelWindow;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
InterfaceTitleTextObject* gTitleText;
CText gText;
CWorld* gGameWorld;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableCheat;
CKeyTable* gKeyTableGame;
CKeyTable* gKeyTableShortcuts;
uint32_t gFirstPersonFlags;
int32_t gSoundBanks[8];

// What gText links to (the font wrappers are held by value; nothing here runs them).
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}

// ---- the test objects, labelled 1 to 14 (see tpw_up_obj)
namespace {
// A mouse mode whose answer is the queue "GetMode" (the original's is a virtual call through the vtable at +0x2c).
class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode", 0));
    }
    void OnAssign() override {}
};

constexpr int kWindows = 4;   // labels 1 (the panel), 2 (another window), 3 (the child), 14 (the previous panel)
alignas(16) unsigned char gWindowStore[kWindows][sizeof(InterfaceWindow)];
CKeyTable gTables[4];                    // labels 4 to 7: camera, cheat, game, shortcuts
alignas(16) unsigned char gMouseStore[sizeof(CMouse)];   // label 8
FakeMode gFakeMode;                      // label 9
alignas(16) unsigned char gTitleStore[sizeof(InterfaceTitleTextObject)];   // label 10
alignas(16) unsigned char gAdvisorStore[sizeof(CAdvisor)];   // label 11
alignas(16) unsigned char gWorldStore[64];   // label 12 (the fake GetAdvisor never reads it)
alignas(16) unsigned char gDrawStore[16];    // what the child's draw object word points at

void* Obj(int32_t label) {
    switch (label) {
        case 1: return gWindowStore[0];
        case 2: return gWindowStore[1];
        case 3: return gWindowStore[2];
        case 14: return gWindowStore[3];
        case 4: case 5: case 6: case 7: return &gTables[label - 4];
        case 8: return gMouseStore;
        case 9: return &gFakeMode;
        case 10: return gTitleStore;
        case 11: return gAdvisorStore;
        case 12: return gWorldStore;
    }
    return nullptr;
}

constexpr const char16_t* kTexts[4] = {nullptr, u"", u"Title", u"A longer title 12é"};
}  // namespace

// ---- the nothrow new: the answer is the queue "New" (0 a NULL, the Mac's failed allocation). The size is recorded as the Mac's (the mouse is
// an empty class there, operator new(1)); the block gets a label and the byte 0xa5.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(InterfaceTitleTextObject)) mac = 0x19c;
    else if (size == sizeof(CMMDefault)) mac = 8;   // the Mac's CMMDefault is a vtable pointer and one word (operator new(8) in the listing); the host's is 16
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the sound, the messages, the keyboard capture
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b, lab(gUIPanelWindow)});
    return 0;
}
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {lab(gUIPanelWindow)}); }
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("TitleColour", {lab(window)}); }
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }

// ---- the key tables
void CKeyTable::Disable() { ev("Disable", {lab(this)}); }
int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    ev("KeyDown", {lab(this), key, modifiers});
    return 0;
}
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    ev("KeyUp", {lab(this), key, modifiers});
    return 0;
}

// ---- the mouse and its default mode
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
CMMDefault::CMMDefault() { ev("CMMDefault.ctor", {lab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

// ---- the title text object: only its constructor is called (a pointer to it goes on as a draw object). The class's bases are not built in
// this library, so the constructor is defined under its symbol name (the complete object constructor, Itanium C1).
extern "C" void tpw_up_title_ctor(InterfaceTitleTextObject* self, void* data) __asm__(
#ifdef __APPLE__
    "__ZN24InterfaceTitleTextObjectC1EPv"
#else
    "_ZN24InterfaceTitleTextObjectC1EPv"
#endif
);
extern "C" void tpw_up_title_ctor(InterfaceTitleTextObject* self, void* data) { ev("TitleText.ctor", {lab(self), lab(data)}); }
void InterfaceDrawObjectSprite::SetPreDrawCallback(int32_t (*callback)(TbRenderArea&)) {
    ev("SetPreDrawCallback", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(callback))});
}

// ---- the window's members the title code calls (none of them is virtual)
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(static_cast<int32_t>(ret("Child", 0))));
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = static_cast<int16_t>(ret("RectL", 0));
    rect->mTop = static_cast<int16_t>(ret("RectT", 0));
    rect->mRight = static_cast<int16_t>(ret("RectR", 0));
    rect->mBottom = static_cast<int16_t>(ret("RectB", 0));
    return 1;
}
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    ev("SetPosition", {lab(this), rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    return 1;
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
// The font is an entry of gText's table: recorded as its index (the Mac's entries are 0x18 apart, the host's are not) and the leftover.
void InterfaceWindow::SetFont(InterfaceFont* font) {
    const InterfaceFontWrapper* entry = static_cast<const InterfaceFontWrapper*>(font);
    ev("SetFont", {lab(this), static_cast<int64_t>(entry - gText.mFonts), 0});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetWindowDrawText(char16_t* text) { ev("SetWindowDrawText", {lab(this)}, {narrow(text).c_str()}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler", 0));
}

// ---- the advisor
CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {lab(this)});
    return static_cast<CAdvisor*>(Obj(static_cast<int32_t>(ret("Advisor", 0))));
}
// The thing id's value is recorded; the Mac passes it in the high half of a word whose low half is stack padding (not recorded).
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("Request.ctor", {newlab(this), message, thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this), lab(&request)});
    return 0;
}
int32_t CAdvisor::CancelMessage(AdvisorMessage message) { ev("CancelMessage", {lab(this), message}); return 0; }
int32_t CAdvisor::PlayLastTutorialMessage() { ev("PlayLastTutorialMessage", {lab(this)}); return 0; }

// ---- the first-person functions
void StopRidingRideIn1stPerson() { ev("StopRidingRideIn1stPerson"); }
void ToggleFirstPersonWalkAround() { ev("ToggleFirstPersonWalkAround"); }

extern "C" {
// The address of the test object with this label (0 for none); the Python side labels it on the host.
void* tpw_up_obj(int32_t label) { return Obj(label); }

// The state a case starts from. in: 0 gUIPanelWindow (a label), 1 gMouse, 2 gMouseMode, 3 and 4 the two mouse words, 5 gTitleText,
// 6 gText's resolution group, 7 gFirstPersonFlags, 8 gGameWorld, 9 the UI sound category (gSoundBanks[3]), 10 whether the child window
// (label 3) has a draw object. The key tables (labels 4 to 7) are always set.
void tpw_up_state(const int32_t* in) {
    std::memset(gWindowStore, 0xa5, sizeof gWindowStore);
    gUIPanelWindow = static_cast<InterfaceWindow*>(Obj(in[0]));
    gMouse = static_cast<CMouse*>(Obj(in[1]));
    gMouseMode = static_cast<CMouseMode*>(Obj(in[2]));
    gMouseWord1E0 = in[3];
    gMouseWord1E4 = in[4];
    gTitleText = static_cast<InterfaceTitleTextObject*>(Obj(in[5]));
    gText.mResolutionGroup = static_cast<uint32_t>(in[6]);
    gFirstPersonFlags = static_cast<uint32_t>(in[7]);
    gGameWorld = static_cast<CWorld*>(Obj(in[8]));
    gSoundBanks[3] = in[9];
    static_cast<InterfaceWindow*>(Obj(3))->mDrawObject = in[10] != 0 ? reinterpret_cast<InterfaceDrawObject*>(gDrawStore) : nullptr;
    gKeyTableCamera = &gTables[0];
    gKeyTableCheat = &gTables[1];
    gKeyTableGame = &gTables[2];
    gKeyTableShortcuts = &gTables[3];
}

// The state after a case, as labels (a made object has one): 0 gUIPanelWindow, 1 gMouse, 2 gMouseMode, 3 and 4 the mouse words,
// 5 gTitleText, 6 the group, 7 the flags, 8 gGameWorld, 9 the sound word, 10 whether the child still has a draw object.
void tpw_up_dump(int32_t* out) {
    out[0] = lab(gUIPanelWindow);
    out[1] = lab(gMouse);
    out[2] = lab(gMouseMode);
    out[3] = gMouseWord1E0;
    out[4] = gMouseWord1E4;
    out[5] = lab(gTitleText);
    out[6] = static_cast<int32_t>(gText.mResolutionGroup);
    out[7] = static_cast<int32_t>(gFirstPersonFlags);
    out[8] = lab(gGameWorld);
    out[9] = gSoundBanks[3];
    out[10] = static_cast<InterfaceWindow*>(Obj(3))->mDrawObject != nullptr;
}

void tpw_up_sound(int32_t code) { UIMain_UISound(code); }
int32_t tpw_up_checkpanel() { return UIMain_CheckPanel() ? 1 : 0; }
void tpw_up_setpanel(int32_t window) { UIMain_SetPanel(static_cast<InterfaceWindow*>(Obj(window))); }
void tpw_up_settitle(int32_t window, int32_t id, int32_t text) {
    UIMain_SetTitleText(static_cast<InterfaceWindow*>(Obj(window)), id, const_cast<char16_t*>(kTexts[text & 3]));
}
void tpw_up_advisor(int32_t message) { UIMain_AdvisorMessage(static_cast<AdvisorMessage>(message)); }
void tpw_up_cancel(int32_t message) { UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(message)); }
int32_t tpw_up_defaultmsg(int32_t window, int32_t message, int32_t a, int32_t b) {
    return UIMain_DefaultPanelMsg(static_cast<InterfaceWindow*>(Obj(window)), message, a, b);
}
void tpw_up_firstperson() { UIMain_GetFullyOutOfFirstPerson(); }
}
