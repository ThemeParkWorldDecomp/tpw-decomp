// Test library for unit w56, the tooltip window (libtpw_unit_UITooltip.dylib; see tests/test_UITooltip.py): src/game/ui/ui_tooltip.cpp (the 15
// functions of the file), with the window's GetOriginalPosition, GetPosition, SetPosition and ShiftPosition (src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp),
// the help text handler the class derives from (src/game/ui/InterfaceHelpText.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp), the window's constructor (src/game/ui/InterfaceWindow.cpp) and the rectangles
// (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp), all real: so the tooltip object's own words and the four test windows' rectangles are compared, as well as the log.
//
// Every other callee is a fake that records through calllog, the same events as the PowerPC side's stubs: the window's other members
// (CreateObject, the font, text and draw object setters, the extra parameters, the callback, Show, Enable and SetText, the default handler),
// the message senders, the pointer, the UI clock, the font table entry and its height, the draw object library, the text sprite's
// constructor, the interface's string table and the multibyte converter.
//
// The world (tpw_tt_op, one extern "C" entry for the set-ups and the calls): four real windows (the first is the root window) built by the
// window constructor, the tooltip object (the static object gUIHelpWindow itself), a default help text object, a font, three draw object
// tokens, a pool of texts and a pool of table strings. The labels: 5 the tooltip object, 6 the default help object, 10 + n window n,
// 20 the font, 30 + n draw object token n, 40 the string table, 100 + n the text sprites the calls make.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbFontRenderParams.h"
#include "TbTextFormatter.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "UI_Base.h"
#include "calllog.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kWindows = 4;
constexpr int kTexts = 8;
constexpr int kStrings = 6;
constexpr int kTokens = 3;
constexpr int kWindowLabel = 10, kFontLabel = 20, kTokenLabel = 30, kTableLabel = 40, kObjectLabel = 5, kDefaultLabel = 6;
constexpr int kWindowBytes = 2048;

alignas(64) unsigned char gWindowStore[kWindows][kWindowBytes];
static_assert(sizeof(InterfaceWindow) <= kWindowBytes, "the window store is too small");
InterfaceWindow* gWindows[kWindows];

// A UTF-16 text as the log keeps it, in a buffer of the caller's (ASCII kept, anything else '?', at most 159 characters). The fakes must
// not allocate: the heap traffic is compared.
void Narrow(const char16_t* text, char (&out)[160]) {
    if (text == nullptr) {
        std::strcpy(out, "(null)");
        return;
    }
    unsigned n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = '\0';
}
void NarrowBytes(const unsigned char* text, char (&out)[160]) {
    if (text == nullptr) {
        std::strcpy(out, "(null)");
        return;
    }
    unsigned n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = '\0';
}

// The pools the case fills: wide texts by number, and the string table's multibyte strings.
const char16_t* gTexts[kTexts];
const unsigned char* gStrings[kStrings];
InterfaceHelpText* gDefaultObject;
alignas(8) unsigned char gTokenStore[kTokens][64];
}  // namespace

// ---- the UI's globals the unit reads or defines (the others live in files that are not linked)
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gUIFocusWindow;
InterfaceHelpText gUIDefaultHelpText;
InterfaceHelpText* gUIHelpText = &gUIDefaultHelpText;
CText gText;

// ---- the senders, the pointer and the clock
// UI_SendMessage answers from the queue "SendMessage": 0 nothing, 1 the answer -1, anything else the address of the help text pair (the low
// word of it, as the host's message 0xc answers).
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    const int64_t code = ret("SendMessage");
    if (code == 0) return 0;
    if (code == 1) return -1;
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(&gUIHelpTextResult));
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
void UI_SetCursor(int16_t cursor) { ev("SetCursor", {cursor}); }
uint32_t IveGotTicks() {
    ev("IveGotTicks");
    return static_cast<uint32_t>(ret("IveGotTicks", 1000));
}

// ---- the window's virtual members that the unit does not translate (CalcClientArea is the base class's: the constructor reaches it)
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
// The two the tooltip calls through the window's vtable.
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::SetText(char16_t* text) {
    char shown[160];
    Narrow(text, shown);
    ev("Window.SetText", {lab(this)}, {shown});
    return 1;
}

// ---- the window's other members the tooltip calls (fakes that record every argument)
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    return 1;
}
// CreateObject answers the window of the queue "CreateObject" (0: NULL, n: the world's window n).
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    const int64_t index = ret("CreateObject");
    return index > 0 && index < kWindows ? gWindows[index] : nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("Window.SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    char shown[160];
    Narrow(text, shown);
    ev("SetClientDrawText", {lab(this)}, {shown});
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 1;
}
// getExtraParam answers the object of the queue "ExtraParam": 0 NULL, 1 the tooltip object, 2 the default help object.
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    const int64_t which = ret("ExtraParam");
    if (which == 1) return &gUIHelpWindow;
    if (which == 2) return gDefaultObject;
    return nullptr;
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == HelpTextMsg ? 1 : callback == nullptr ? 0 : 2});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

// ---- the font table entry (a font whose height the queue "Height" gives), the draw object library and the string table
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
namespace {
InterfaceFont* gFont;
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return gFont;
}
int32_t InterfaceFont::GetHeight() {
    ev("Font.GetHeight", {lab(this)});
    return static_cast<int32_t>(ret("Height"));
}
// FindDrawObject answers the token of the queue "FindDraw" (0: NULL, n: token n - 1).
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    const int64_t index = ret("FindDraw");
    return index > 0 && index <= kTokens ? reinterpret_cast<InterfaceDrawObject*>(gTokenStore[index - 1]) : nullptr;
}
// GetConstString answers the string of the queue "String" (an index into the pool; -1 NULL).
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    const int64_t index = ret("String", -1);
    return index >= 0 && index < kStrings ? gStrings[index] : nullptr;
}
// UIMain_ConvertMulti: the low bytes of the multibyte text widened into a buffer (NULL: an empty text).
namespace {
char16_t gConvertBuffer[0x200];
}
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    char shown[160];
    NarrowBytes(text, shown);
    ev("ConvertMulti", {}, {shown});
    unsigned n = 0;
    for (; text != nullptr && text[n] != 0 && n < 0x1ff; ++n) gConvertBuffer[n] = text[n];
    gConvertBuffer[n] = 0;
    return gConvertBuffer;
}

// ---- the text sprite the tooltip window gets as its client draw object (made by `new`; the Mac's constructor is one fake, so the base
// classes' constructors and the formatter the C++ object holds record nothing)
InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
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
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    ev("TextSprite.ctor", {newlab(this), lab(data)});
}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}

// ---- the world
namespace {
// The window's rectangles in the Python module's order: the four coordinates of each of the four rectangles, the pivot and the depth, the
// animation's offsets.
void DumpWindow(const InterfaceWindow* w, int64_t*& out) {
    const CoolRect* rects[4] = {&w->mRect, &w->mOriginalRect, &w->mPreviousRect, &w->mClientRect};
    for (const CoolRect* rect : rects) {
        *out++ = rect->mLeft;
        *out++ = rect->mTop;
        *out++ = rect->mRight;
        *out++ = rect->mBottom;
    }
    *out++ = w->mPivotX;
    *out++ = w->mPivotY;
    *out++ = w->mZ;
    for (int i = 0; i < 4; ++i) *out++ = w->mAnimOffset[i];
}

void* gObjectVptr;
}  // namespace

extern "C" {

// Starts a case: the four windows (the first is the root; each is 0xa5 first, then built by the window constructor with the given
// rectangle, none with a parent), the objects, the texts and the strings. `rects` is 16 shorts; `texts` and `strings` are pointers
// to the case's tables (kTexts and kStrings of them). Recording is off while this runs.
void tpw_tt_world(const int16_t* rects, const char16_t* const* texts, const unsigned char* const* strings) {
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    if (gObjectVptr == nullptr) std::memcpy(&gObjectVptr, static_cast<void*>(&gUIHelpWindow), sizeof gObjectVptr);
    for (int i = 0; i < kWindows; ++i) {
        std::memset(gWindowStore[i], 0xa5, kWindowBytes);
        gWindows[i] = new (gWindowStore[i]) InterfaceWindow(nullptr, 1, 0x100 + i, rects[4 * i], rects[4 * i + 1], rects[4 * i + 2],
                                                            rects[4 * i + 3]);
        // markers in the previous rectangle (the constructor leaves it as it was) and in the animation offsets
        gWindows[i]->mPreviousRect.mLeft = static_cast<int16_t>(0x1111 + i);
        gWindows[i]->mPreviousRect.mTop = static_cast<int16_t>(0x2222 + i);
        gWindows[i]->mPreviousRect.mRight = static_cast<int16_t>(0x3333 + i);
        gWindows[i]->mPreviousRect.mBottom = static_cast<int16_t>(0x4444 + i);
        tpw_cl_label(kWindowLabel + i, gWindows[i]);
    }
    gUIRootWindow = gWindows[0];
    for (int i = 0; i < kTexts; ++i) gTexts[i] = texts[i];
    for (int i = 0; i < kStrings; ++i) gStrings[i] = strings[i];
    gDefaultObject = &gUIDefaultHelpText;
    gUIHelpText = &gUIDefaultHelpText;
    static InterfaceFontWrapper font;
    gFont = &font;
    tpw_cl_label(kObjectLabel, &gUIHelpWindow);
    tpw_cl_label(kDefaultLabel, &gUIDefaultHelpText);
    tpw_cl_label(kFontLabel, gFont);
    for (int i = 0; i < kTokens; ++i) tpw_cl_label(kTokenLabel + i, gTokenStore[i]);
    gText.mResourcePointers[7] = &gText.mResourcePointers[8];
    tpw_cl_label(kTableLabel, gText.mResourcePointers[7]);
    gUIHelpTextResult = 0;
    gUIHelpTextString = nullptr;
    gUIRealScreenHeight = 0x300;
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    tpw_cl_on = was;
}

// Sets the window's animation offsets (four shorts).
void tpw_tt_anim(int32_t slot, const int16_t* offsets) {
    for (int i = 0; i < 4; ++i) gWindows[slot]->mAnimOffset[i] = offsets[i];
}

// Sets the window's original rectangle (left, top, right, bottom).
void tpw_tt_orig(int32_t slot, const int16_t* rect) {
    gWindows[slot]->mOriginalRect.mLeft = rect[0];
    gWindows[slot]->mOriginalRect.mTop = rect[1];
    gWindows[slot]->mOriginalRect.mRight = rect[2];
    gWindows[slot]->mOriginalRect.mBottom = rect[3];
}

// The tooltip object's words: enabled, wanted, mouse x and y, initialised, the tick, the window the pointer is in and the text window (slot
// numbers, -1 for none).
void tpw_tt_object(const int32_t* w) {
    gUIHelpWindow.mEnabled = static_cast<int16_t>(w[0]);
    gUIHelpWindow.mWanted = static_cast<int16_t>(w[1]);
    gUIHelpWindow.mMouseX = static_cast<int16_t>(w[2]);
    gUIHelpWindow.mMouseY = static_cast<int16_t>(w[3]);
    gUIHelpWindow.mShowing = static_cast<int16_t>(w[4]);
    gUIHelpWindow.mMovedInTicks = static_cast<uint32_t>(w[5]);
    gUIHelpWindow.mWindow = w[6] >= 0 ? gWindows[w[6]] : nullptr;
    gUIHelpWindow.mHelpWindow = w[7] >= 0 ? gWindows[w[7]] : nullptr;
}

// The help text pair message 0xc answers: the number, and the text (a pool number, -1 NULL).
void tpw_tt_pair(int32_t number, int32_t text) {
    gUIHelpTextResult = number;
    gUIHelpTextString = text >= 0 ? const_cast<char16_t*>(gTexts[text]) : nullptr;
}

void tpw_tt_screen(int32_t height) { gUIRealScreenHeight = static_cast<int16_t>(height); }

// The handler the pump has: 0 the default object, 1 the tooltip object.
void tpw_tt_handler(int32_t which) { gUIHelpText = which == 0 ? &gUIDefaultHelpText : &gUIHelpWindow; }

// One call. Codes: 1 Init, 2 DeInit, 3 HelpTextMsg(window slot, message, a, b), 4 ShowHelpText, 5 HideHelpText, 6 OnMouseMovedIn(slot, -1
// for NULL), 7 OnFrame, 8 Enable, 9 Disable, 10 Update, 11 InitHelpText, 12 SetHelpTextString(text), 13 MoveHelpText(x, y), 14 the
// destructor (no delete), 15 GetOriginalPosition(slot, 1 for a NULL rectangle), 16 the static constructor over 0xa5. `out` gets the answer
// (the call's, then the rectangle of code 15).
void tpw_tt_op(int32_t code, const int64_t* a, int64_t* out) {
    for (int i = 0; i < 5; ++i) out[i] = 0;
    switch (code) {
    case 1: gUIHelpWindow.Init(); break;
    case 2: gUIHelpWindow.DeInit(); break;
    case 3: out[0] = HelpTextMsg(gWindows[a[0]], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 4: gUIHelpWindow.ShowHelpText(); break;
    case 5: gUIHelpWindow.HideHelpText(); break;
    case 6: gUIHelpWindow.OnMouseMovedIn(a[0] >= 0 ? gWindows[a[0]] : nullptr); break;
    case 7: gUIHelpWindow.OnFrame(); break;
    case 8: gUIHelpWindow.Enable(); break;
    case 9: gUIHelpWindow.Disable(); break;
    case 10: gUIHelpWindow.Update(); break;
    case 11: InitHelpText(); break;
    case 12: SetHelpTextString(const_cast<char16_t*>(gTexts[a[0]])); break;
    case 13: MoveHelpText(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1])); break;
    case 14:
        gUIHelpWindow.~CUIHelpWindow();
        std::memcpy(static_cast<void*>(&gUIHelpWindow), &gObjectVptr, sizeof gObjectVptr);   // (the object is used again)
        break;
    case 15: {
        CoolRect rect;
        rect.mLeft = 0x5151;
        rect.mTop = 0x5252;
        rect.mRight = 0x5353;
        rect.mBottom = 0x5454;
        out[0] = gWindows[a[0]]->GetOriginalPosition(a[1] != 0 ? nullptr : &rect);
        out[1] = rect.mLeft;
        out[2] = rect.mTop;
        out[3] = rect.mRight;
        out[4] = rect.mBottom;
        break;
    }
    case 16:
        std::memset(static_cast<void*>(&gUIHelpWindow), 0xa5, sizeof gUIHelpWindow);
        new (&gUIHelpWindow) CUIHelpWindow();
        break;
    }
}

// The state in the Python module's order: the object's words (the vtable is the class's: 1), the pair, the handler, the four windows, the
// heap traffic.
int32_t tpw_tt_state(int64_t* out) {
    int64_t* start = out;
    void* vptr;
    std::memcpy(&vptr, static_cast<void*>(&gUIHelpWindow), sizeof vptr);
    *out++ = gUIHelpWindow.mEnabled;
    *out++ = gUIHelpWindow.mWanted;
    *out++ = gUIHelpWindow.mMouseX;
    *out++ = gUIHelpWindow.mMouseY;
    *out++ = gUIHelpWindow.mShowing;
    *out++ = gUIHelpWindow.mMovedInTicks;
    *out++ = vptr == gObjectVptr ? 1 : 0;
    *out++ = lab(gUIHelpWindow.mWindow);
    *out++ = lab(gUIHelpWindow.mHelpWindow);
    *out++ = gUIHelpTextResult;
    *out++ = lab(gUIHelpText);
    for (int i = 0; i < kWindows; ++i) DumpWindow(gWindows[i], out);
    for (int i = 0; i < 4; ++i) *out++ = tpw_cl_heap[i];
    return static_cast<int32_t>(out - start);
}

}  // extern "C"
