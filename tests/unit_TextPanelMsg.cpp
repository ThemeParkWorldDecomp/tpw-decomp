// Test library for the online news panel (unit v5225; libtpw_unit_TextPanelMsg.dylib, tests/test_TextPanelMsg.py), main app:
//   src/game/ui/ui_onlineNewsPanel.cpp   TextPanelMsg, CreateOnlineNewsPanel, CalcScrollBar and the panel's four globals
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. The windows are
// zeroed blocks with a hand made vtable (the slots SetValue and SetText); the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CLBLobby* gLobby = nullptr;
CFSOnlineSystem* gOnlineSystem = nullptr;
int16_t gUIRealScreenHeight = 0;
int16_t gUIRealScreenWidth = 0;

// ---- the unit's own globals and functions
extern uint32_t gOnlineNewsFont;
extern int16_t gOnlineNewsScript[];
extern uint8_t gOnlineNewsShowInbox;
extern InterfaceWindow* gOnlineNewsPanel;
int32_t NewsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
int32_t TextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
void CalcScrollBar();
void CreateOnlineNewsPanel();

// A callback the panel hands to Dialog (its real body is in another unit).
int32_t NewsPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

InterfaceWindow* gWin[6];            // 0 the dialog, 1..5 the windows 0x5b7f to 0x5b83
InterfaceFont* gFont = nullptr;
const char16_t* gStr[3];             // the strings 0x10b to 0x10d
char16_t* gSysTextA = nullptr;       // what a made online system holds at 0xa0 and 0xbc
char16_t* gSysTextB = nullptr;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// The slots of the hand made window vtable (Itanium: the dtor takes two slots, SetValue is 8 and SetText 10).
int32_t VtSetValue(void* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return static_cast<int32_t>(ret("SetValue"));
}

int32_t VtSetText(void* self, char16_t* text) {
    ev("SetText", {lab(self)}, {narrow(text).c_str()});
    return 1;
}

void* gVtable[16];

}  // namespace

// Constructors whose class vtables this library does not link: defined under the mangled names.
extern "C" void FakeCoolRectCtor(CoolRect* self) __asm__("__ZN8CoolRectC1Ev");
extern "C" void FakeCoolRectCtor(CoolRect* self) {
    self->mLeft = 0;
    self->mTop = 0;
    self->mRight = 0;
    self->mBottom = 0;
}

extern "C" void FakeCoolRectDtor(CoolRect*) __asm__("__ZN8CoolRectD1Ev");
extern "C" void FakeCoolRectDtor(CoolRect*) {}

extern "C" void FakeSpriteCtor(InterfaceTextObjectSprite* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(InterfaceTextObjectSprite* self, void* data) {
    ev("Sprite", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}

CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem", {newlab(this)});
    mF98.mData = gSysTextA;
    mFB4.mData = gSysTextB;
}

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int32_t slot = id - 0x5b7e;
    return (slot >= 1 && slot <= 5) ? gWin[slot] : nullptr;
}

void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }

void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }

void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }

void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), lab(reinterpret_cast<const void*>(callback))});
}

void InterfaceWindow::SetTextOffset(int32_t x, int32_t y) { ev("SetTextOffset", {lab(this), x, y}); }

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this)});
    rect->mLeft = static_cast<int16_t>(ret("crect_l"));
    rect->mTop = static_cast<int16_t>(ret("crect_t"));
    rect->mRight = static_cast<int16_t>(ret("crect_r"));
    rect->mBottom = static_cast<int16_t>(ret("crect_b"));
    return 1;
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}

void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), min, max}); }

int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 1;
}

int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {lab(this)});
    return static_cast<int32_t>(ret("GetHeight"));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return gFont;
}

InterfaceWindow* Dialog(int16_t* script, InterfaceWindowCallback callback, int32_t modal) {
    ev("Dialog", {lab(script), lab(reinterpret_cast<const void*>(callback)), modal});
    return gWin[0];
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    int32_t slot = id - 0x10b;
    return (slot >= 0 && slot <= 2) ? gStr[slot] : nullptr;
}

void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("UIMain_SetTitleText", {lab(window), id}, {narrow(text).c_str()});
}

void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* size) {
    ev("UI_MeasureText", {font, horizontal, vertical, wrap}, {narrow(text).c_str()});
    ev("MeasureRect", {rect.left, rect.top, rect.right, rect.bottom, size->left, size->top, size->right, size->bottom});
    size->bottom = static_cast<int32_t>(ret("mt_bottom"));
}

extern "C" {

// The host address of a global or function by the module's name for it (ADDR's keys).
uint64_t tpw_nsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", &gOnlineNewsFont},
        {"script", gOnlineNewsScript},
        {"showinbox", &gOnlineNewsShowInbox},
        {"panel", &gOnlineNewsPanel},
        {"lobby", &gLobby},
        {"sys", &gOnlineSystem},
        {"height", &gUIRealScreenHeight},
        {"width", &gUIRealScreenWidth},
        {"newsmsg", reinterpret_cast<const void*>(&NewsPanelMsg)},
        {"textmsg", reinterpret_cast<const void*>(&TextPanelMsg)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_nsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// One window: a zeroed block with the fake vtable and the z the case wants.
uint64_t tpw_nsp_newwin(int32_t z) {
    gVtable[0] = nullptr;
    gVtable[1] = nullptr;
    gVtable[2 + 8] = reinterpret_cast<void*>(&VtSetValue);
    gVtable[2 + 10] = reinterpret_cast<void*>(&VtSetText);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(sizeof(InterfaceWindow)));
    *reinterpret_cast<void***>(window) = &gVtable[2];
    window->mZ = static_cast<int16_t>(z);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

// The case's world: the six windows, the font block, the three strings, the two texts a made online system holds.
void tpw_nsp_world(uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4, uint64_t w5, uint64_t font, uint64_t s0,
                   uint64_t s1, uint64_t s2, uint64_t texta, uint64_t textb) {
    gWin[0] = reinterpret_cast<InterfaceWindow*>(w0);
    gWin[1] = reinterpret_cast<InterfaceWindow*>(w1);
    gWin[2] = reinterpret_cast<InterfaceWindow*>(w2);
    gWin[3] = reinterpret_cast<InterfaceWindow*>(w3);
    gWin[4] = reinterpret_cast<InterfaceWindow*>(w4);
    gWin[5] = reinterpret_cast<InterfaceWindow*>(w5);
    gFont = reinterpret_cast<InterfaceFont*>(font);
    gStr[0] = reinterpret_cast<const char16_t*>(s0);
    gStr[1] = reinterpret_cast<const char16_t*>(s1);
    gStr[2] = reinterpret_cast<const char16_t*>(s2);
    gSysTextA = reinterpret_cast<char16_t*>(texta);
    gSysTextB = reinterpret_cast<char16_t*>(textb);
}

// An online system that exists already (made without the constructor, so no event).
uint64_t tpw_nsp_makesys(uint64_t texta, uint64_t textb) {
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
    system->mF98.mData = reinterpret_cast<char16_t*>(texta);
    system->mFB4.mData = reinterpret_cast<char16_t*>(textb);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(system));
}

// What a made online system holds, read back for the compare (0 when none).
int32_t tpw_nsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(int16_t));
    return n;
}

// ---- the functions
int32_t tpw_nsp_textpanelmsg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return TextPanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

void tpw_nsp_create() { CreateOnlineNewsPanel(); }

void tpw_nsp_calc() { CalcScrollBar(); }

}  // extern "C"
