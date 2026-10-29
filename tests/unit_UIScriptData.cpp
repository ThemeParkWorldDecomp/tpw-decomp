// Test library for unit w36 (libtpw_unit_UIScriptData.dylib; see tests/test_UIScriptData.py): the island lobby controller's message
// function and its small companions (src/game/ui/ui_islandLobbyController.cpp), the window classes' WhatAmI members, CalcClientArea and
// CallDefaultWindowHandler (src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceButton.cpp, src/game/ui/InterfaceEditBox.cpp, src/game/ui/InterfaceImage.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceLightPanel.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp, src/game/ui/InterfaceTextBox.cpp, src/game/ui/InterfaceVaryBox.cpp, src/game/ui/InterfaceWindow.cpp), the help text handler and the timer constructor (src/game/ui/InterfaceHelpText.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp), and the login
// panel's script (src/game/ui/ui_playerLogin.cpp). The lobby's and the island controller's scripts live in src/game/online/lobby.cpp and
// src/game/ui/ui_islandLobbyController.cpp, src/game/ui/ui_onlineOutbox.cpp, whose libraries (unit_Lobby, unit_IslandLobby) the Python module reads them from.
//
// Under them, real: the script interpreter (src/game/ui/InterfaceWindow.cpp), the control factory and constructors
// (src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceButton.cpp, src/game/ui/InterfaceEditBox.cpp, src/game/ui/InterfaceImage.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceLightPanel.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp, src/game/ui/InterfaceTextBox.cpp, src/game/ui/InterfaceVaryBox.cpp, src/game/ui/InterfaceWindow.cpp), the control setters (src/game/ui/CoolRegion.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp), the region shapes
// (src/game/ui/CoolRegion.cpp), the window constructor (src/game/ui/InterfaceWindow.cpp) and the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). So a script builds
// its windows here the way the game does, through WhatAmI.
//
// Everything else is a fake that records through calllog: the message senders, the pointer, the timer table, the draw objects (a
// draw object library that makes one per hash), the particle emitters, the window's other virtual members, the controls' untranslated
// virtual members (empty, except the three CalcClientArea overrides the constructors reach: those do what the originals do), the
// screens the controller's buttons move, the UI clock, the assertion and the allocator.
//
// Two worlds, driven through extern "C": the TREE (tpw_sd_tree builds a script under a new root window and dumps every window it made)
// and the OPERATIONS (tpw_sd_op: a handful of windows, help text objects, timers, the screens and the controller's globals, with the
// state dumped after each operation by tpw_sd_state).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <typeinfo>

namespace {
bool gObserve;
void ObserveAllocation(std::size_t size, bool array, void* block);
}  // namespace
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "rsse.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kWindows = 4;      // the operations' windows
constexpr int kHelps = 4;        // the help text objects: the default one, then three the operations make
constexpr int kTimers = 2;
constexpr int kTimerBlock = 64;
constexpr int kHelpBlock = 64;
constexpr int kWindowBlock = 640;
constexpr int kMaxDraws = 24;
constexpr int kWindowLabel = 10, kHelpLabel = 20, kTimerLabel = 30, kScreenLabel = 40, kDefaultLabel = 50;

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!gObserve) return;
    std::memset(block, 0xa5, size);
    if (!array) newlab(block);
}
}  // namespace

// ---- the UI's globals the unit reads or defines (the others live in files that are not linked)
bool gUIHebrew;
int32_t gUIAutoRedraw;
InterfaceWindow* gUIKeyboardWindow;
InterfaceHelpText gUIDefaultHelpText;
InterfaceHelpText* gUIHelpText = &gUIDefaultHelpText;
InterfaceWindow* gIslandLobbyController;
int32_t gIslandLobbyKeyEmitter;
CLBScreenSystem* gLobbyScreens;

// ---- the senders, the pointer, the clock and the timers
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
uint32_t IveGotTicks() {
    ev("IveGotTicks");
    return static_cast<uint32_t>(ret("Ticks", 1000));
}
InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id) {
    ev("AddTimer", {lab(window), slice, id});
    return nullptr;
}
void InterfaceStringHeap::ReleaseString(char16_t* text) { ev("ReleaseString", {lab(text)}); }

// ---- the window handlers: never called by the unit, stored by the constructors (only their addresses matter)
int32_t UI_DefaultWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultButtonCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultCheckButtonsCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultEditBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultKnobCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultListControl2Callback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultTextBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultVaryBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {
// The two handlers the operations give windows (they record and answer from the queue "Default"): the default handler of a window
// and a second one. Both are told apart by their number.
int32_t HandlerA(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Handler", {1, lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default", 0x55));
}
int32_t HandlerB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Handler", {2, lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default", 0x66));
}
InterfaceWindowCallback CallbackOf(int64_t code) {
    return code == 1 ? HandlerA : code == 2 ? HandlerB : code == 3 ? UI_DefaultThumbtrackCallback : nullptr;
}

// Which handler a pointer is, as the Python module's table numbers them: 0 none, 1 and 2 the operations' two, 3 the thumb's, 4 the
// window's default, 5 the button's, 6 check buttons, 7 edit box, 8 knob, 9 list box, 10 list control, 11 text box, 12 vary box,
// 99 any other.
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    if (callback == HandlerA) return 1;
    if (callback == HandlerB) return 2;
    if (callback == UI_DefaultThumbtrackCallback) return 3;
    if (callback == UI_DefaultWindowCallback) return 4;
    if (callback == UI_DefaultButtonCallback) return 5;
    if (callback == UI_DefaultCheckButtonsCallback) return 6;
    if (callback == UI_DefaultEditBoxCallback) return 7;
    if (callback == UI_DefaultKnobCallback) return 8;
    if (callback == UI_DefaultListBoxCallback) return 9;
    if (callback == UI_DefaultListControl2Callback) return 10;
    if (callback == UI_DefaultTextBoxCallback) return 11;
    if (callback == UI_DefaultVaryBoxCallback) return 12;
    return 99;
}
}  // namespace

// ---- the assertion and the allocator (CoolPoly allocates with new (std::nothrow); the queue "NewFails" makes a call answer NULL)
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (ret("NewFails") != 0) return nullptr;
    return operator new(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return operator new[](size); }

// ---- the window's untranslated members. CalcClientArea, WhatAmI and CallDefaultWindowHandler are the unit's.
void InterfaceWindow::DrawFunction() {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    return static_cast<int16_t>(ret("Window.Enable"));
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("Window.SetText", {lab(this), lab(text)});
    return 1;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    return 1;
}
// The first child with that id (src/game/ui/InterfaceWindow.cpp's, which is not linked; it records nothing).
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext)
        if (child->mId == id) return child;
    return nullptr;
}
// The rectangle the window has (the real one takes the animation's offsets off it when asked to; the operations leave them at 0).
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = mRect.mLeft;
    rect->mTop = mRect.mTop;
    rect->mRight = mRect.mRight;
    rect->mBottom = mRect.mBottom;
    return 1;
}

// ---- the scroll bar (never made by the unit's scripts): the constructor and members that the vtable needs
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                       int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    ev("ScrollBar.ctor", {lab(this), lab(parent), attributes, id, left, top, right, bottom});
}
void InterfaceScrollBar::SetMinMax(int32_t a, int32_t b) { ev("ScrollBar.SetMinMax", {lab(this), a, b}); }
void InterfaceScrollBar::DrawFunction() { InterfaceWindow::DrawFunction(); }
void InterfaceScrollBar::CalcClientArea() { InterfaceWindow::CalcClientArea(); }
InterfaceScrollBar::~InterfaceScrollBar() {}
int32_t InterfaceScrollBar::WhatAmI() { return kUIKindScrollBar; }
void InterfaceScrollBar::ClientDraw() { InterfaceWindow::ClientDraw(); }
int16_t InterfaceScrollBar::Enable(int16_t on) { return InterfaceWindow::Enable(on); }
int32_t InterfaceScrollBar::SetValue(int32_t value) { return InterfaceWindow::SetValue(value); }
int32_t InterfaceScrollBar::GetValue() { return InterfaceWindow::GetValue(); }

// ---- the controls' untranslated virtual members (as in tests/unit_UIControls.cpp, which tests the constructors). The three
// CalcClientArea overrides do what the originals do, which the PowerPC side runs for real: they record nothing.
void InterfaceBar::DrawFunction() {}
InterfaceBar::~InterfaceBar() {}
int32_t InterfaceBar::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceBar::SetValue(int32_t) { return 1; }
int32_t InterfaceBar::GetValue() { return mValue; }

InterfaceCheckButtons::~InterfaceCheckButtons() {}

void InterfaceEditBox::DrawFunction() {}
void InterfaceEditBox::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceEditBox::~InterfaceEditBox() {}
void InterfaceEditBox::ClientDraw() {}
int32_t InterfaceEditBox::SetText(char16_t*) { return 1; }

InterfaceImage::~InterfaceImage() {}
int32_t InterfaceImage::SetValue(int32_t) { return 1; }
int32_t InterfaceImage::GetValue() { return mValue; }

InterfaceKnob::~InterfaceKnob() {}
int32_t InterfaceKnob::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceKnob::SetValue(int32_t) { return 1; }
int32_t InterfaceKnob::GetValue() { return mValue; }

InterfaceLightPanel::~InterfaceLightPanel() {}
int32_t InterfaceLightPanel::SetValue(int32_t) { return 1; }
int32_t InterfaceLightPanel::GetValue() { return mValue; }

void InterfaceListBox::DrawFunction() {}
// The client rectangle is the window's less 0x30 on the right, and the list rectangle is that inset by 4 on every side.
void InterfaceListBox::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mClientRect.mRight = static_cast<int16_t>(mClientRect.mRight - 0x30);
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
    mListRect.mLeft = static_cast<int16_t>(mListRect.mLeft + 4);
    mListRect.mRight = static_cast<int16_t>(mListRect.mRight - 4);
    mListRect.mTop = static_cast<int16_t>(mListRect.mTop + 4);
    mListRect.mBottom = static_cast<int16_t>(mListRect.mBottom - 4);
}
InterfaceListBox::~InterfaceListBox() {}
void InterfaceListBox::ClientDraw() {}

// The client rectangle is the window's, and so is the list rectangle.
void InterfaceListControl2::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
}
InterfaceListControl2::~InterfaceListControl2() {}
void InterfaceListControl2::ClientDraw() {}

void InterfaceTextBox::DrawFunction() {}
InterfaceTextBox::~InterfaceTextBox() {}
void InterfaceTextBox::ClientDraw() {}
int32_t InterfaceTextBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceTextBox::SetText(char16_t*) { return 1; }

InterfaceVaryBox::~InterfaceVaryBox() {}
int32_t InterfaceVaryBox::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceVaryBox::SetValue(int32_t) { return 1; }
int32_t InterfaceVaryBox::GetValue() { return mValue; }

// ---- the fonts (a script's windows have none) and the particle emitters
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}

namespace {
int32_t gNextEmitter = 7;
}
void UpdateIslandLobbyKeyDisplay() { ev("UpdateIslandLobbyKeyDisplay"); }
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(gNextEmitter++);
}
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }

// ---- the screens the controller's buttons move. Each virtual member records (the base classes' constructors and destructors do
// nothing); the controller reaches them through the screen system.
CLBScreenComponent::CLBScreenComponent() {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return 0; }
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
void CLBScreenComponentThemeIslands::Show() { ev("Theme.Show", {lab(this)}); }
void CLBScreenComponentThemeIslands::Hide() { ev("Theme.Hide", {lab(this)}); }
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {}
void CLBScreenComponentThemeIslands::GotoNextIsland() { ev("Theme.GotoNextIsland", {lab(this)}); }
void CLBScreenComponentThemeIslands::GotoPreviousIsland() { ev("Theme.GotoPreviousIsland", {lab(this)}); }
void CLBScreenComponentThemeIslands::SelectCurrentIsland() { ev("Theme.SelectCurrentIsland", {lab(this)}); }
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() {}
CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe() {}
void CLBScreenComponentDownloadGlobe::Init() {}
void CLBScreenComponentDownloadGlobe::Tick() {}
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentDownloadGlobe::Show() { ev("Globe.Show", {lab(this)}); }
void CLBScreenComponentDownloadGlobe::Hide() {}

namespace {
CLBScreenComponentThemeIslands gTheme;
CLBScreenComponentDownloadGlobe gGlobe;
alignas(16) unsigned char gScreensBlock[sizeof(CLBScreenSystem)];
}  // namespace

// ---- the draw objects: the draw object library makes one per hash (named with the next label when first asked for); Change records
// what the window told it
namespace {
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    uint32_t mHash = 0;
};
FakeDraw gDraws[kMaxDraws];
int gDrawCount;

// Which rectangle of which window a rectangle is: 10 * the window's label + 1 its own rectangle, 2 the original, 3 the previous,
// 4 the client rectangle, 5 the one at 0x34; -1 for any other. (The window is found from the rectangle's address and has to carry a
// label: every window made with operator new, and the operations' windows, do.)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int64_t RectCode(const CoolRect* rect) {
    struct Member {
        std::size_t offset;
        int code;
    };
    const Member members[] = {{offsetof(InterfaceWindow, mRect), 1},        {offsetof(InterfaceWindow, mOriginalRect), 2},
                              {offsetof(InterfaceWindow, mPreviousRect), 3}, {offsetof(InterfaceWindow, mClientRect), 4},
                              {offsetof(InterfaceWindow, mRect34), 5}};
    for (const Member& m : members) {
        const auto* window = reinterpret_cast<const InterfaceWindow*>(reinterpret_cast<const unsigned char*>(rect) - m.offset);
        if (lab(window) > 0) return 10 * static_cast<int64_t>(lab(window)) + m.code;
    }
    return -1;
}
#pragma clang diagnostic pop
}  // namespace

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t hash) {
    ev("FindDrawObject", {static_cast<int64_t>(hash)});
    for (int i = 0; i < gDrawCount; ++i)
        if (gDraws[i].mHash == hash) return &gDraws[i];
    if (gDrawCount >= kMaxDraws) return nullptr;
    FakeDraw* draw = &gDraws[gDrawCount++];
    draw->mHash = hash;
    newlab(draw);
    return draw;
}

InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    ev("Change", {lab(this), flags, change->mFrame, change->mDepth, change->mF08, RectCode(change->mRect), change->mRect->mLeft,
                  change->mRect->mTop, change->mRect->mRight, change->mRect->mBottom, change->mF0C, change->mF0E});
    if ((flags & 0x10000) != 0) ev("Change.style", {lab(this), change->mText->mF06[2]});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }   // retyped by w35c
void InterfaceDrawObject::Show(int16_t shown) { ev("Draw.Show", {lab(this), shown}); }
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

namespace {
// ---- the tree dump: one record per window, depth first (a window, its children, then its next sibling at the same depth)
struct Out {
    int64_t* p;
    int64_t n = 0;
    void put(int64_t value) {
        *p++ = value;
        ++n;
    }
};

int64_t HashOf(const InterfaceDrawObject* draw) {
    if (draw == nullptr) return 0;
    for (int i = 0; i < gDrawCount; ++i)
        if (&gDraws[i] == draw) return gDraws[i].mHash;
    return -1;
}

void DumpRegion(const InterfaceWindow* w, Out& o) {
    const CoolRegion* region = w->mRegion;
    if (region == nullptr) {
        for (int i = 0; i < 5; ++i) o.put(0);
    } else if (region == &w->mRect) {
        o.put(1);
        for (int i = 0; i < 4; ++i) o.put(0);
    } else if (typeid(*region) == typeid(CoolRect)) {
        const auto* rect = static_cast<const CoolRect*>(region);
        o.put(3);
        o.put(rect->mLeft);
        o.put(rect->mTop);
        o.put(rect->mRight);
        o.put(rect->mBottom);
    } else if (typeid(*region) == typeid(CoolCircle)) {
        const auto* circle = static_cast<const CoolCircle*>(region);
        o.put(4);
        o.put(circle->mX);
        o.put(circle->mY);
        o.put(circle->mRadius);
        o.put(0);
    } else if (typeid(*region) == typeid(CoolEllipse)) {
        const auto* ellipse = static_cast<const CoolEllipse*>(region);
        o.put(5);
        o.put(ellipse->mX);
        o.put(ellipse->mY);
        o.put(ellipse->mRadiusX);
        o.put(ellipse->mRadiusY);
    } else if (typeid(*region) == typeid(CoolPoly)) {
        const auto* poly = static_cast<const CoolPoly*>(region);
        o.put(6);
        o.put(poly->mCount);
        o.put(poly->mPoints != nullptr);
        o.put(0);
        o.put(0);
        if (poly->mPoints != nullptr && poly->mCount > 0)
            for (int i = 0; i < 2 * poly->mCount; ++i) o.put(poly->mPoints[i]);
    } else {
        o.put(2);
        for (int i = 0; i < 4; ++i) o.put(0);
    }
}

void DumpRect(const CoolRect& r, Out& o) {
    o.put(r.mLeft);
    o.put(r.mTop);
    o.put(r.mRight);
    o.put(r.mBottom);
}

int64_t Colour(const UI_RGBA& c) {
    return (static_cast<int64_t>(c.mR) << 24) | (static_cast<int64_t>(c.mG) << 16) | (static_cast<int64_t>(c.mB) << 8) | c.mA;
}

// The window's fields in the Python module's order. The kind is what WhatAmI answers through the vtable.
void DumpWindow(InterfaceWindow* w, int depth, Out& o) {
    const int32_t kind = w->WhatAmI();
    o.put(depth);
    o.put(lab(w));
    o.put(kind);
    o.put(w->mId);
    o.put(static_cast<int32_t>(w->mAttributes));
    DumpRect(w->mRect, o);
    DumpRect(w->mClientRect, o);
    DumpRect(w->mOriginalRect, o);
    o.put(w->mZ);
    o.put(w->mZRot);
    o.put(w->mPivotX);
    o.put(w->mPivotY);
    o.put(w->mHelpText);
    o.put(w->mF90);
    DumpRegion(w, o);
    o.put(HashOf(w->mDrawObject));
    o.put(HashOf(w->mClientDrawObject));
    o.put(CallbackCode(w->mCallback));
    o.put(CallbackCode(w->mDefaultCallback));
    o.put(w->mCursor);
    o.put(lab(w->mParent));
    switch (kind) {
    case kUIKindButton: {
        const auto* b = static_cast<const InterfaceButton*>(w);
        o.put(b->mState);
        o.put(b->mHilight);
        o.put(b->mRepeatDelay);
        o.put(b->mRepeatInterval);
        o.put(b->mText != nullptr);
        o.put(Colour(b->mTextUpColour));
        o.put(Colour(b->mTextDownColour));
        o.put(b->mTextShiftX);
        o.put(b->mTextShiftY);
        o.put(b->mHeldState);
        break;
    }
    case kUIKindEditBox: {
        const auto* b = static_cast<const InterfaceEditBox*>(w);
        o.put(b->mBuffer != nullptr);
        o.put(b->mMaxLength);
        o.put(b->mTimer != nullptr);
        o.put(b->mRightToLeft);
        break;
    }
    case kUIKindListBox:
        DumpRect(static_cast<const InterfaceListBox*>(w)->mListRect, o);
        break;
    case kUIKindListControl: {
        const auto* b = static_cast<const InterfaceListControl2*>(w);
        DumpRect(b->mListRect, o);
        o.put(b->mColumnCount);
        o.put(b->mColumns != nullptr);
        if (b->mColumns != nullptr && b->mColumnCount > 0 && b->mColumnCount <= 8)
            for (int i = 0; i < b->mColumnCount; ++i) DumpRect(b->mColumns[i], o);
        break;
    }
    case kUIKindKnob: {
        const auto* b = static_cast<const InterfaceKnob*>(w);
        o.put(b->mMinAngle);
        o.put(b->mMaxAngle);
        o.put(b->mAngleRange);
        break;
    }
    case kUIKindVaryBox: {
        const auto* b = static_cast<const InterfaceVaryBox*>(w);
        o.put(b->mMinValue);
        o.put(b->mMaxValue);
        o.put(b->mValue);
        o.put(b->mIncrement);
        o.put(b->mForceUpdate);
        break;
    }
    case kUIKindLightPanel:
        o.put(static_cast<const InterfaceLightPanel*>(w)->mValue);
        break;
    case kUIKindTextBox: {
        const auto* b = static_cast<const InterfaceTextBox*>(w);
        o.put(b->mText != nullptr);
        o.put(b->mTextLength);
        break;
    }
    case kUIKindCheckButtons:
        o.put(static_cast<const InterfaceCheckButtons*>(w)->mCurrent);
        break;
    default:
        break;
    }
}

void DumpTree(InterfaceWindow* w, int depth, Out& o) {
    for (; w != nullptr; w = w->mNext) {
        DumpWindow(w, depth, o);
        DumpTree(w->mChild, depth + 1, o);
    }
}

// ---- the operations' world
alignas(16) unsigned char gWindowBlocks[kWindows][kWindowBlock];
InterfaceWindow* gWindow[kWindows];
alignas(16) unsigned char gHelpBlocks[kHelps][kHelpBlock];
InterfaceHelpText* gHelp[kHelps];
alignas(16) unsigned char gTimerBlocks[kTimers][kTimerBlock];
InterfaceTimer* gTimer[kTimers];
int64_t gHeap[4];
int gHelpKind[kHelps];   // 0 not made, 1 the base class, 2 a class that records HideHelpText
int gHandlerSlot;        // the slot of the help text object UI_SetHelpTextHandler last installed (0: the default instance)

// A help text handler that records its HideHelpText (the game's own handler class overrides it).
class RecordingHelp : public InterfaceHelpText {
public:
    void HideHelpText() override {
        ev("Help.HideHelpText", {lab(this)});
        InterfaceHelpText::HideHelpText();
    }
};
static_assert(sizeof(RecordingHelp) <= kHelpBlock, "help block too small");
static_assert(sizeof(InterfaceTimer) <= kTimerBlock, "timer block too small");
static_assert(sizeof(InterfaceListControl2) <= kWindowBlock, "window block too small");

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kWindows ? gWindow[slot] : nullptr; }
InterfaceHelpText* Help(int64_t slot) { return slot == 0 ? &gUIDefaultHelpText : slot > 0 && slot < kHelps ? gHelp[slot] : nullptr; }
}  // namespace

extern "C" {

// ---- the TREE ---------------------------------------------------------------------------------------------------------------

// Resets the globals the windows' scripts use.
void tpw_sd_reset() {
    gObserve = false;
    gDrawCount = 0;
    for (FakeDraw& draw : gDraws) {
        draw.mHash = 0;
        draw.mInLibrary = 0;
        draw.mResizable = 0;
        draw.mShown = 0;
    }
    gUIKeyboardWindow = nullptr;
    gUIScriptPosition = 0;
    gUILastWindow = nullptr;
    gUIHebrew = false;
    gUIAutoRedraw = 0;
    gIslandLobbyController = nullptr;
    gIslandLobbyKeyEmitter = 0;
    gNextEmitter = 7;
    std::memset(gHeap, 0, sizeof gHeap);
}

// Makes a root window (the constructor, rectangle 0, 0, 2048, 1536, attribute 1, parent none), builds `script` under it with the
// callback `callbackCode` (0 none, 1 and 2 the operations' handlers) and dumps the tree. out[0] is the label of the window Build
// answered, out[1] the script position after it, out[2] the label of the keyboard window, out[3] the number of words that follow
// (the tree's, then the draw objects' (hash, in library, resizable, shown), the heap traffic and three globals). Answers the number of words written.
int32_t tpw_sd_tree(const int16_t* script, int32_t callbackCode, int64_t* out) {
    const int32_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    gObserve = true;
    auto* root = new InterfaceWindow(nullptr, 1, 0, 0, 0, 2048, 1536);
    InterfaceWindow* made = root->Build(const_cast<int16_t*>(script), CallbackOf(callbackCode));
    gObserve = false;
    Out o{out + 4};
    DumpTree(root, 0, o);
    out[0] = lab(made);
    out[1] = gUIScriptPosition;
    out[2] = lab(gUIKeyboardWindow);
    out[3] = o.n;
    for (int i = 0; i < gDrawCount; ++i) {
        o.put(gDraws[i].mHash);
        o.put(gDraws[i].mInLibrary);
        o.put(gDraws[i].mResizable);
        o.put(gDraws[i].mShown);
    }
    o.put(tpw_cl_heap[0] - heapBefore[0]);
    o.put(tpw_cl_heap[1] - heapBefore[1]);
    o.put(tpw_cl_heap[2] - heapBefore[2]);
    o.put(tpw_cl_heap[3] - heapBefore[3]);
    o.put(lab(gUILastWindow));
    o.put(gUIHebrew);
    o.put(gUIAutoRedraw);
    return static_cast<int32_t>(o.n + 4);
}

// ---- the OPERATIONS ---------------------------------------------------------------------------------------------------------

// Resets the operations' world: no windows, help texts or timers, the controller's globals clear, no screens.
void tpw_sd_ops_reset() {
    tpw_sd_reset();
    for (int i = 0; i < kWindows; ++i) gWindow[i] = nullptr;
    for (int i = 0; i < kHelps; ++i) {
        gHelp[i] = nullptr;
        gHelpKind[i] = 0;
    }
    for (int i = 0; i < kTimers; ++i) gTimer[i] = nullptr;
    gLobbyScreens = nullptr;
    gUIHelpText = &gUIDefaultHelpText;
    gHandlerSlot = 0;
    // the default instance is the constructor's object, as the load made it (every field the constructor leaves alone is 0 here);
    // operation 30 makes it again over 0xa5 bytes when the constructor's own work is to be seen
    gUIDefaultHelpText.~InterfaceHelpText();
    std::memset(static_cast<void*>(&gUIDefaultHelpText), 0, sizeof gUIDefaultHelpText);
    new (&gUIDefaultHelpText) InterfaceHelpText;
}

// Names the world's fixed objects for the call log (windows, help texts and timers are named when made).
void tpw_sd_labels() {
    tpw_cl_label(kScreenLabel, &gTheme);
    tpw_cl_label(kScreenLabel + 1, &gGlobe);
    tpw_cl_label(kDefaultLabel, &gUIDefaultHelpText);
}

// One operation; a[] are its arguments, out[0..4] the answer. The numbers are those of the Python module.
void tpw_sd_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    const int32_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    gObserve = true;
    InterfaceWindow* w = Window(a[0]);
    switch (op) {
    case 1: {   // a window through the real constructor: slot, parent slot, attributes, id, left, top, right, bottom, default handler
        unsigned char* block = gWindowBlocks[a[0]];
        std::memset(block, 0xa5, kWindowBlock);
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);   // (before the constructor, which records events)
        gWindow[a[0]] = new (block) InterfaceWindow(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                                    static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]),
                                                    static_cast<int16_t>(a[7]));
        if (a[8] >= 0) gWindow[a[0]]->mDefaultCallback = CallbackOf(a[8]);   // (-1 keeps the constructor's)
        out[0] = lab(gWindow[a[0]]);
        break;
    }
    case 2:   // the rectangle (a raw store): slot, left, top, right, bottom
        w->mRect.mLeft = static_cast<int16_t>(a[1]);
        w->mRect.mTop = static_cast<int16_t>(a[2]);
        w->mRect.mRight = static_cast<int16_t>(a[3]);
        w->mRect.mBottom = static_cast<int16_t>(a[4]);
        break;
    case 3:   // the default handler: slot, handler
        w->mDefaultCallback = CallbackOf(a[1]);
        break;
    case 4:   // the controller's global: slot (-1 none)
        gIslandLobbyController = Window(a[0]);
        break;
    case 5:   // the key emitter's handle
        gIslandLobbyKeyEmitter = static_cast<int32_t>(a[0]);
        break;
    case 6: {   // the screens: 0 none, else present
        if (a[0] == 0) {
            gLobbyScreens = nullptr;
        } else {
            std::memset(gScreensBlock, 0, sizeof gScreensBlock);
            auto* screens = reinterpret_cast<CLBScreenSystem*>(gScreensBlock);
            screens->mThemeIslands = &gTheme;
            screens->mDownloadGlobe = &gGlobe;
            gLobbyScreens = screens;
        }
        break;
    }
    case 10:   // IslandLobbyControllerMsg: slot, message, a, b
        out[0] = IslandLobbyControllerMsg(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
        break;
    case 11:
        RemoveIslandLobbyController();
        break;
    case 12:
        HideIslandLobbyController();
        break;
    case 13:   // UpdateKeyParticles: slot (-1 none), on
        UpdateKeyParticles(w, a[1] != 0);
        break;
    case 14:   // CalcClientArea
        w->CalcClientArea();
        break;
    case 15:   // CallDefaultWindowHandler: slot, message, a, b
        out[0] = w->CallDefaultWindowHandler(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
        break;
    case 16:   // WhatAmI
        out[0] = w->WhatAmI();
        break;
    case 20: {   // a help text object through the constructor: slot (1 to 3), class (1 base, 2 recording)
        unsigned char* block = gHelpBlocks[a[0]];
        std::memset(block, 0xa5, kHelpBlock);
        tpw_cl_label(kHelpLabel + static_cast<int32_t>(a[0]), block);
        if (a[1] == 2) gHelp[a[0]] = new (block) RecordingHelp;
        else gHelp[a[0]] = new (block) InterfaceHelpText;
        gHelpKind[a[0]] = static_cast<int>(a[1]);
        break;
    }
    case 21: {   // a heap help text: slot (1 to 3), class; made with operator new (the storage is filled first)
        void* block = operator new(kHelpBlock);
        std::memset(block, 0xa5, kHelpBlock);
        tpw_cl_label(kHelpLabel + static_cast<int32_t>(a[0]), block);
        if (a[1] == 2) gHelp[a[0]] = new (block) RecordingHelp;
        else gHelp[a[0]] = new (block) InterfaceHelpText;
        gHelpKind[a[0]] = static_cast<int>(a[1]);
        break;
    }
    case 22: {   // the destructor: slot, flag (positive: the object is deleted too)
        InterfaceHelpText* h = Help(a[0]);
        if (static_cast<int16_t>(a[1]) > 0) delete h;   // (the original tests the low 16 bits of the flag)
        else h->~InterfaceHelpText();
        gHelp[a[0]] = nullptr;
        gHelpKind[a[0]] = 0;
        break;
    }
    case 23:   // UI_SetHelpTextHandler: slot (-1 none, 0 the default instance); the slot of the object that became the handler is kept
        UI_SetHelpTextHandler(a[0] < 0 ? nullptr : Help(a[0]));
        gHandlerSlot = gUIHelpText == &gUIDefaultHelpText ? 0 : -1;
        for (int i = 1; i < kHelps; ++i)
            if (gHelp[i] != nullptr && gUIHelpText == gHelp[i]) gHandlerSlot = i;
        break;
    case 24: {   // a member of a help text: slot, which, arguments
        InterfaceHelpText* h = Help(a[0]);
        switch (a[1]) {
        case 0: h->ShowHelpText(); break;
        case 1: h->HideHelpText(); break;
        case 2: h->Enable(); break;
        case 3: h->Disable(); break;
        case 4: h->Show(); break;
        case 5: h->Hide(); break;
        case 6: h->Update(); break;
        case 7: h->OnMouseMoved(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3])); break;
        case 8: h->OnMouseMovedIn(Window(a[2])); break;
        case 9: h->OnMouseMovedOut(); break;
        case 10: h->OnMouseDown(); break;
        case 11: h->OnFrame(); break;
        case 12: h->OnHideOrDelete(Window(a[2])); break;
        case 13: h->Init(); break;
        case 14: h->DeInit(); break;
        default: break;
        }
        break;
    }
    case 25:   // a field of a help text, stored raw: slot, which (0 enabled, 1 wanted, 2 showing), value
        if (a[1] == 0) Help(a[0])->mEnabled = static_cast<int16_t>(a[2]);
        else if (a[1] == 1) Help(a[0])->mWanted = static_cast<int16_t>(a[2]);
        else Help(a[0])->mShowing = static_cast<int16_t>(a[2]);
        break;
    case 26: {   // a timer through the constructor: slot
        unsigned char* block = gTimerBlocks[a[0]];
        std::memset(block, 0xa5, kTimerBlock);
        tpw_cl_label(kTimerLabel + static_cast<int32_t>(a[0]), block);
        gTimer[a[0]] = new (block) InterfaceTimer;
        break;
    }
    case 30: {   // the default help text re-made by its constructor
        gUIDefaultHelpText.~InterfaceHelpText();
        std::memset(static_cast<void*>(&gUIDefaultHelpText), 0xa5, sizeof gUIDefaultHelpText);
        new (&gUIDefaultHelpText) InterfaceHelpText;
        break;
    }
    default:
        break;
    }
    gObserve = false;
    for (int i = 0; i < 4; ++i) gHeap[i] += tpw_cl_heap[i] - heapBefore[i];
}

// The state: the windows (rectangles, handlers, the fields the unit's functions could touch), the controller's globals, the help text
// handler and the help texts, the timers, the heap traffic. Answers the number of words.
int32_t tpw_sd_state(int64_t* out) {
    Out o{out};
    for (int i = 0; i < kWindows; ++i) {
        InterfaceWindow* w = gWindow[i];
        o.put(w != nullptr);
        if (w == nullptr) continue;
        DumpRect(w->mRect, o);
        DumpRect(w->mOriginalRect, o);
        DumpRect(w->mPreviousRect, o);
        DumpRect(w->mClientRect, o);
        DumpRect(w->mRect34, o);
        o.put(CallbackCode(w->mCallback));
        o.put(CallbackCode(w->mDefaultCallback));
        o.put(static_cast<int32_t>(w->mAttributes));
        o.put(w->mId);
        o.put(w->mZ);
        o.put(lab(w->mParent));
    }
    o.put(lab(gIslandLobbyController));
    o.put(gIslandLobbyKeyEmitter);
    o.put(gLobbyScreens != nullptr);
    o.put(gHandlerSlot);
    for (int i = 0; i < kHelps; ++i) {
        InterfaceHelpText* h = i == 0 ? &gUIDefaultHelpText : gHelp[i];
        o.put(h != nullptr);
        if (h == nullptr) continue;
        o.put(h->mEnabled);
        o.put(h->mWanted);
        o.put(h->mMouseX);
        o.put(h->mMouseY);
        o.put(h->mShowing);
        o.put(h->mMovedInTicks);
        o.put(lab(h->mWindow));
    }
    for (int i = 0; i < kTimers; ++i) {
        InterfaceTimer* t = gTimer[i];
        o.put(t != nullptr);
        if (t == nullptr) continue;
        o.put(lab(t->mWindow));
        o.put(t->mStart);
        o.put(t->mSlice);
        o.put(t->mId);
        o.put(t->mRunning);
        o.put(t->mFree);
        o.put(lab(t->mNext));
        o.put(lab(t->mPrevious));
    }
    for (int i = 0; i < 4; ++i) o.put(gHeap[i]);
    o.put(tpw_cl_n);
    return static_cast<int32_t>(o.n);
}

// The login panel's description as the library holds it (the Python module reads its 371 words and compares them with the image).
const int16_t* tpw_sd_login_pointer() { return gLoginPanelWindowData; }

}  // extern "C"
