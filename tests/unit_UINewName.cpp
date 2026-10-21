// Test library for unit w43b (libtpw_unit_UINewName.dylib; see tests/test_UINewName.py): src/ui_newname.cpp (NewName),
// src/ui_newname_data.cpp (the dialog's window description), src/ui_dialog.cpp (Dialog), src/InterfaceWindow_z.cpp (setZ and
// GetClientRect), src/InterfaceCheckButtons_current.cpp (SetCurrent) and the rectangles of src/UI_Base.cpp.
//
// Everything else the five functions call is a fake that records through calllog: the window's setters, GetWindow (the first child with the
// id, as the original), the script builder and the window factory (they answer the windows the test queues), the highest depth and the
// window's position, the edit box's members, the buttons' SetState, the draw objects (Change records the depth of a flag 2 change), the
// draw object library, the font table, the string table's answer, the title colour, the advisor's Add and the three text objects'
// constructors. The Python module drives it through tpw_nn_run and reads the windows back with tpw_nn_getwin.
//
// The world: 16 windows (check buttons objects, so that every one can be the check buttons; 15 is the interface's root window), 16 draw
// objects, 13 fonts, 8 wide texts, the advisor and the screens.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"

#include "Advisor.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbFontRenderParams.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kWindows = 16;
constexpr int kRoot = 15;
constexpr int kDraws = 16;
constexpr int kFonts = 13;
constexpr int kTexts = 8;
constexpr int kTextUnits = 64;

// (The labels of the world's fixed objects are the Python module's: windows 10 to 25, draw objects 30 to 45, fonts 50 to 62, texts 70 to 77,
// the name buffer 80, the advisor 81 and the screens 82.)

// The windows are check buttons objects built with the fake constructors; the test sets the fields the five functions read.
InterfaceCheckButtons* gWin[kWindows];
alignas(InterfaceCheckButtons) unsigned char gWinStorage[kWindows][sizeof(InterfaceCheckButtons)];

class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
};
FakeDraw gDraws[kDraws];
InterfaceFont gFonts[kFonts];
char16_t gTexts[kTexts][kTextUnits];
alignas(16) unsigned char gScreensBlock[sizeof(CLBScreenSystem)];
alignas(16) unsigned char gAdvisorBlock[sizeof(CLBScreenComponentAdvisor)];
}  // namespace

// ---- globals of other files
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gLoginPanelWindow;
CLBScreenSystem* gLobbyScreens;

// Two message functions (only their addresses matter): the dialog's own, and any other.
int32_t CreatePlayerMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
namespace {
int32_t OtherMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// 20 the dialog's message function, 99 any other, 0 none (as the Python module's table).
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    if (callback == CreatePlayerMsg) return 20;
    return 99;
}
int64_t WindowIndex(const void* window) {
    for (int i = 0; i < kWindows; ++i)
        if (window == gWin[i]) return i;
    return window == nullptr ? -1 : -2;
}
// A queue's answer (an index plus one, 0 for NULL) as a pointer into a pool.
template <typename T>
T* FromQueue(const char* key, T* pool[], int count) {
    const int64_t index = ret(key);
    return index >= 1 && index <= count ? pool[index - 1] : nullptr;
}
}  // namespace

// ---- the window's members the unit's functions do not own. Each one records its arguments.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}   // the fake leaves the object as it finds it
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceCheckButtons::InterfaceCheckButtons(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                             int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
InterfaceCheckButtons::~InterfaceCheckButtons() {}
int32_t InterfaceCheckButtons::WhatAmI() { return 8; }

// The virtual SetText (slot 0x2c): the text's label and its characters.
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this), lab(text)}, {narrow(text).c_str()});
    return 1;
}
// The first child with that id (the original's loop).
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext)
        if (child->mId == id) return child;
    return nullptr;
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {lab(this), horizontal, vertical}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int64_t>(reinterpret_cast<intptr_t>(value))});
    return 1;
}
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}, {narrow(text).c_str()}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), CallbackCode(callback)});
}
// The script builder answers the window the test queues; the description is told apart as NULL (0), the dialog's (1) or other (2).
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == nullptr ? 0 : description == gNewNameWindowData ? 1 : 2, CallbackCode(callback)});
    return FromQueue("Build", reinterpret_cast<InterfaceWindow**>(gWin), kWindows);
}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return FromQueue("CreateObject", reinterpret_cast<InterfaceWindow**>(gWin), kWindows);
}
// The position the queue "GetPosition" gives (left, top, right, bottom, in that order).
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), rect != nullptr, subtract});
    if (rect == nullptr) return 0;
    rect->mLeft = static_cast<int16_t>(ret("GetPosition"));
    rect->mTop = static_cast<int16_t>(ret("GetPosition"));
    rect->mRight = static_cast<int16_t>(ret("GetPosition"));
    rect->mBottom = static_cast<int16_t>(ret("GetPosition"));
    return 1;
}
// The highest depth: the queue's answer; the window (a label) and the rectangle's words are recorded.
int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect) {
    ev("UI_GetHighestZ",
       {lab(window), rect != nullptr, rect != nullptr ? rect->mLeft : 0, rect != nullptr ? rect->mTop : 0,
        rect != nullptr ? rect->mRight : 0, rect != nullptr ? rect->mBottom : 0});
    return static_cast<int32_t>(ret("HighestZ"));
}

// ---- the buttons and the edit box
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("Button.SetState", {lab(this), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 1;
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("EditBox.SetStaticTextArea", {lab(this), lab(text), capacity});
    return 1;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("EditBox.SetCursorDrawObject", {lab(this), lab(object)}); }
void InterfaceEditBox::SelectAll() { ev("EditBox.SelectAll", {lab(this)}); }
void InterfaceEditBox::SetUnAcceptedChars(char16_t* text) { ev("EditBox.SetUnAcceptedChars", {lab(this), lab(text)}, {narrow(text).c_str()}); }

// ---- the draw objects: Change records the depth of a depth change
InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    ev("Change", {lab(this), flags, (flags & 2) != 0 ? change->mDepth : 0});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// The library answers the draw object the queue "Find" names (an index plus one; 0 for NULL); the name is recorded.
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    const int64_t index = ret("Find");
    return index >= 1 && index <= kDraws ? &gDraws[index - 1] : nullptr;
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t) { return nullptr; }

// ---- the text objects (made by `new`: each constructor gives its object the next label and says whether it was given data). The sprite
// object under them and the members it holds by value are fakes that record nothing; the vtables need them.
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
namespace {
bool gDerivedCtor;   // set by a derived class's constructor for the base's: the original's constructors do not call each other's records
void* Silent(void* data) {
    gDerivedCtor = true;
    return data;
}
}  // namespace
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    if (gDerivedCtor)
        gDerivedCtor = false;
    else
        ev("TextObject.ctor", {newlab(this), data != nullptr});
}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(Silent(data)) {
    ev("TitleText.ctor", {newlab(this), data != nullptr});
}
InterfaceTitleTextObject::~InterfaceTitleTextObject() {}
void InterfaceTitleTextObject::UpdateTextures(char16_t*) {}
InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void* data) : InterfaceTextObjectSprite(Silent(data)) {
    ev("CursorText.ctor", {newlab(this), data != nullptr});
}
InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite() {}
intptr_t InterfaceCursorPosTextObjectSprite::Query(uint32_t, intptr_t) { return 0; }
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() {}
TbTextFormatter::~TbTextFormatter() {}

// ---- the font table, the string table's answer, the title colour and the advisor
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return index < static_cast<uint32_t>(kFonts) ? &gFonts[index] : nullptr;
}
// The interface's string `id`: the text the queue "GetString:<id>" names (an index plus one, 0 for NULL); a number without a queue says the
// text numbered by its low three bits.
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    const int64_t index = ret("GetString", id, 1 + (id & 7));
    return index >= 1 && index <= kTexts ? gTexts[index - 1] : nullptr;
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {lab(window)}); }
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) { ev("Advisor.Add", {lab(this), response.mId, clearFirst}); }

// ---- the driver
extern "C" {
void* tpw_nn_window(int i) { return gWin[i]; }
void* tpw_nn_draw(int i) { return &gDraws[i]; }
void* tpw_nn_font(int i) { return &gFonts[i]; }
void* tpw_nn_text(int i) { return gTexts[i]; }
void* tpw_nn_buffer() { return gNewNameText; }
void* tpw_nn_advisor() { return gAdvisorBlock; }
void* tpw_nn_screens() { return gScreensBlock; }
int16_t* tpw_nn_script() { return gNewNameWindowData; }

// Text number i is the characters given (a NUL follows).
void tpw_nn_settext(int i, const uint16_t* chars, int n) {
    for (int k = 0; k < n && k < kTextUnits - 1; ++k) gTexts[i][k] = chars[k];
    gTexts[i][n < kTextUnits - 1 ? n : kTextUnits - 1] = 0;
}

// Makes the world: every window made again with the fake constructor and everything but its vtable pointer cleared.
void tpw_nn_reset() {
    for (int i = 0; i < kWindows; ++i) {
        gWin[i] = new (gWinStorage[i]) InterfaceCheckButtons(nullptr, 0, 0, 0, 0, 0, 0);
        std::memset(reinterpret_cast<unsigned char*>(gWin[i]) + sizeof(void*), 0, sizeof(InterfaceCheckButtons) - sizeof(void*));
    }
    std::memset(gNewNameText, 0, sizeof gNewNameText);
    std::memset(gScreensBlock, 0, sizeof gScreensBlock);
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(gScreensBlock);
    gLobbyScreens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(gAdvisorBlock);
    gUIRootWindow = gWin[kRoot];
    gLoginPanelWindow = nullptr;
    gNewNameWindow = nullptr;
}

// Window i: its depth, draw object and client draw object (-1 for none), first child and next sibling (-1 for none), id, the check
// buttons' current id and the client rectangle (left, top, right, bottom).
void tpw_nn_setwin(int i, int z, int draw, int client, int child, int next, int id, int current, const int16_t* rect) {
    InterfaceCheckButtons* w = gWin[i];
    w->mZ = static_cast<int16_t>(z);
    w->mDrawObject = draw < 0 ? nullptr : &gDraws[draw];
    w->mClientDrawObject = client < 0 ? nullptr : &gDraws[client];
    w->mChild = child < 0 ? nullptr : gWin[child];
    w->mNext = next < 0 ? nullptr : gWin[next];
    w->mId = id;
    w->mCurrent = current;
    w->mClientRect.mLeft = rect[0];
    w->mClientRect.mTop = rect[1];
    w->mClientRect.mRight = rect[2];
    w->mClientRect.mBottom = rect[3];
}

// The state of window i: its depth and the check buttons' current id.
void tpw_nn_getwin(int i, int32_t* out) {
    out[0] = gWin[i]->mZ;
    out[1] = gWin[i]->mCurrent;
}

// The world's globals: the login panel's window (a window index, -1 for none) and the name entry dialog's (the same).
void tpw_nn_globals(int panel, int dialog) {
    gLoginPanelWindow = panel < 0 ? nullptr : gWin[panel];
    gNewNameWindow = dialog < 0 ? nullptr : gWin[dialog];
}

// The window the name entry global holds, as an index (-1 for NULL, -2 for any other pointer).
int tpw_nn_dialog_index() { return static_cast<int>(WindowIndex(gNewNameWindow)); }

// Runs one of the functions. op 0: NewName(a). op 1: Dialog(description a (0 none, 1 the dialog's, 2 other), callback b (0 none, 1
// CreatePlayerMsg, 2 any other), modal c); out[0] is the window index it answers. op 2: setZ(window a, z b). op 3: SetCurrent(window a,
// id b); out[0] is the answer. op 4: GetClientRect(window a, rect pointer present b); out[0] is the answer and out[1..4] the rectangle
// (given as the words of out[1..4] before the call). op 5: SetClientRect(window a, rect pointer present b) with the rectangle out[1..4];
// out[0] is the answer and out[1..4] the window's client rectangle after the call.
void tpw_nn_run(int op, int64_t a, int64_t b, int64_t c, int32_t* out) {
    static int16_t other[4] = {1, 2, 3, 4};
    switch (op) {
    case 0: NewName(static_cast<int32_t>(a)); break;
    case 1: {
        int16_t* description = a == 0 ? nullptr : a == 1 ? gNewNameWindowData : other;
        InterfaceWindowCallback callback = b == 0 ? nullptr : b == 1 ? CreatePlayerMsg : OtherMsg;
        out[0] = static_cast<int32_t>(WindowIndex(Dialog(description, callback, static_cast<int32_t>(c))));
        break;
    }
    case 2: gWin[a]->setZ(static_cast<int16_t>(b)); break;
    case 3: out[0] = gWin[a]->SetCurrent(static_cast<int32_t>(b)); break;
    case 4: {
        CoolRect rect;
        rect.mLeft = static_cast<int16_t>(out[1]);
        rect.mTop = static_cast<int16_t>(out[2]);
        rect.mRight = static_cast<int16_t>(out[3]);
        rect.mBottom = static_cast<int16_t>(out[4]);
        out[0] = gWin[a]->GetClientRect(b != 0 ? &rect : nullptr);
        out[1] = rect.mLeft;
        out[2] = rect.mTop;
        out[3] = rect.mRight;
        out[4] = rect.mBottom;
        break;
    }
    case 5: {
        CoolRect rect;
        rect.mLeft = static_cast<int16_t>(out[1]);
        rect.mTop = static_cast<int16_t>(out[2]);
        rect.mRight = static_cast<int16_t>(out[3]);
        rect.mBottom = static_cast<int16_t>(out[4]);
        out[0] = gWin[a]->SetClientRect(b != 0 ? &rect : nullptr);
        out[1] = gWin[a]->mClientRect.mLeft;
        out[2] = gWin[a]->mClientRect.mTop;
        out[3] = gWin[a]->mClientRect.mRight;
        out[4] = gWin[a]->mClientRect.mBottom;
        break;
    }
    }
}
}
