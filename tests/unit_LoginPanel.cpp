// Test library for unit w39, the login panel (libtpw_unit_LoginPanel.dylib; see tests/test_LoginPanel.py): src/ui_loginpanel.cpp
// (CreateLoginPanel, FillLoginNames, UI_TITLE_COLOUR, UIMain_GetString), src/InterfaceWindow_font.cpp (SetFont, SetFontJust,
// SetFontColour, setExtraParam, SetClientDrawText and the virtual SetText) and src/keytables_disable.cpp (DisableAllKeytables).
//
// Real under them, so that CreateLoginPanel builds the panel the way the game does (the window tree is compared with the original's):
// the login panel's script (src/ui_loginpanel_data.cpp), the script interpreter (src/InterfaceWindow_script.cpp), the control factory and
// constructors (src/ui_controls_create.cpp, src/InterfaceButton.cpp, src/ui_controls_small.cpp, src/ui_cool_shapes.cpp), the window
// constructor (src/InterfaceWindow.cpp), WhatAmI and CalcClientArea (src/ui_whatami.cpp) and the rectangles (src/UI_Base.cpp).
//
// Everything else is a fake that records through calllog: Show, the message senders, the pointer, the draw object library (a draw
// object per hash) and the draw objects (Change, with the part of the change its flags select, and Show), the title text object's
// constructor, the font table, the user information (its constructor and the three members the panel asks), the advisor's Add and the
// islands' ResetIslandLoopingSounds, the string table's GetConstString and the converter's ConvertString, the timer table, the string
// heap, the assertion and the allocator.
//
// The world (built by tpw_lp_world): a root window (the interface's), the lobby's window under it, four windows of the tests with no
// parent, the lobby and its screens (plain blocks), 13 fonts, a pool of eight fake draw objects, a pool of wide texts, the string
// table, the seven key tables with 16 entries each, and the panel's globals. The Python module drives it through tpw_lp_op and reads
// the state with tpw_lp_state.
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
#define TPW_CALLLOG_LABELS 4096   // every panel names about forty windows and draw objects
#include "calllog.cpp"

#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kTestWindows = 4;
constexpr int kFonts = 13;
constexpr int kPoolDraws = 8;
constexpr int kMaxDraws = 24;        // the draw object library's (one per hash)
constexpr int kTexts = 16;           // the wide texts the operations hand out
constexpr int kTextUnits = 128;
constexpr int kMultibyte = 8;        // the string table's texts
constexpr int kStrings = 24;         // the string table's entries
constexpr int kTables = 7;
constexpr int kEntries = 16;
constexpr int kWindowBlock = 640;
constexpr int kNames = 4;
constexpr int kStateLabels = 4096;
constexpr int kBufferWords = 64;     // the converted-text buffer's first characters, compared

// The labels of the world's fixed objects.
constexpr int kRootLabel = 10, kLobbyWindowLabel = 11, kTestLabel = 12;   // the test windows are 12 to 15
constexpr int kLobbyLabel = 16, kScreensLabel = 17, kThemeLabel = 18, kAdvisorLabel = 19, kUserInfoLabel = 20;
constexpr int kTableLabel = 21, kConverterLabel = 22, kBufferLabel = 23;
constexpr int kFontLabel = 30;       // 30 to 42
constexpr int kPoolDrawLabel = 50;   // 50 to 57

// ---- the world
alignas(16) unsigned char gWindowBlocks[2 + kTestWindows][kWindowBlock];
InterfaceWindow* gRoot;
InterfaceWindow* gLobbyWindow;
InterfaceWindow* gTest[kTestWindows];
alignas(16) unsigned char gLobbyBlock[sizeof(CLBLobby)];
alignas(16) unsigned char gScreensBlock[sizeof(CLBScreenSystem)];
alignas(16) unsigned char gThemeBlock[sizeof(CLBScreenComponentThemeIslands)];
alignas(16) unsigned char gAdvisorBlock[sizeof(CLBScreenComponentAdvisor)];
alignas(16) unsigned char gUserInfoBlock[sizeof(CFSUserInfo)];
alignas(16) unsigned char gTableBlock[64];       // the string table and the converters: only their addresses are used
alignas(16) unsigned char gConverterBlock[64];
InterfaceFont gFonts[kFonts];
char16_t gTexts[kTexts][kTextUnits];
bool gTextSet[kTexts];
struct StringEntry {
    bool present;
    int32_t id;
    unsigned char bytes[64];
};
StringEntry gStrings[kStrings];
unsigned char gMultibyte[kMultibyte][64];
bool gMultibyteSet[kMultibyte];
TbStringBase<char16_t> gNameViews[kNames];
bool gNameSet[kNames];
int32_t gEasy[kNames];
CKeyTableEntry gEntries[kTables][kEntries];
CKeyTable gTables[kTables];
CKeyTable** const gTableGlobals[kTables] = {&gKeyTable,     &gKeyTableGame,      &gKeyTableCamera, &gKeyTableCheat,
                                            &gKeyTableCoaster, &gKeyTableShortcuts, &gKeyTableDebug};
int16_t gScriptFirstWord;   // the script's first word as the image has it (the operation 28 tampers with it)
int64_t gHeap[4];
bool gHaveScriptWord;

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!gObserve) return;
    std::memset(block, 0xa5, size);
    if (!array) newlab(block);
}
}  // namespace

// ---- globals of other files
CText gText;
CLBLobby* gLobby;
CLBScreenSystem* gLobbyScreens;
CFSUserInfo* gUserInfo;
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gUIKeyboardWindow;
bool gUIHebrew;
int32_t gUIAutoRedraw;
char16_t gUIMainConvertBuffer[0x4000];

InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() : mGrowBy(0) {}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

// ---- the senders, the pointer, the clock and the timers
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id) {
    ev("AddTimer", {lab(window), slice, id});
    return nullptr;
}
void InterfaceStringHeap::ReleaseString(char16_t* text) { ev("ReleaseString", {lab(text)}); }

// ---- the window handlers: never called by the unit, stored by the constructors (only their addresses matter); the panel's three
// message functions are told apart by their number
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
int32_t LoginPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t EntryMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t QuitMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {
// Which handler a pointer is, as the Python module's table numbers them: 0 none, 3 the thumb's, 4 the window's default, 5 the button's,
// 6 check buttons, 7 edit box, 8 knob, 9 list box, 10 list control, 11 text box, 12 vary box, 20 to 22 the panel's three, 99 any other.
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
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
    if (callback == LoginPanelMsg) return 20;
    if (callback == EntryMsg) return 21;
    if (callback == QuitMsg) return 22;
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

// ---- the window's untranslated members. CalcClientArea and WhatAmI are src/ui_whatami.cpp's, SetText is the unit's.
void InterfaceWindow::DrawFunction() {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
// Show records, and (the queue "ShowDropsUserInfo") may make the user information vanish at once, as a window message handler could.
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    if (ret("ShowDropsUserInfo") != 0) gUserInfo = nullptr;
    return 1;
}
// The first child with that id (src/InterfaceWindow_show.cpp's, which is not linked; it records nothing).
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext)
        if (child->mId == id) return child;
    return nullptr;
}

// ---- the scroll bar (never made by the unit's script): the constructor and members that the vtable needs
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                       int16_t bottom)
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

// ---- the controls' untranslated virtual members (as in tests/unit_UIScriptData.cpp). The three CalcClientArea overrides do what the
// originals do, which the PowerPC side runs for real: they record nothing.
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

// ---- the draw objects. The draw state (what the window's setters told it) is kept by label; Change records the part of the change that
// its flags select (the original builds the change on its stack and fills in only that part).
namespace {
struct DrawState {
    bool used;
    int32_t shown;     // the last Show's argument, -1 when never shown
    int32_t changes;   // how many times Change was called
    int32_t font;      // the style's font byte (flag 0x10000)
    int32_t just[2];   // flag 0xc00
    int32_t colour[4];   // flag 0x40000
    int32_t hasText;   // 1 once flag 0x100 gave a text (2: the text was NULL)
    char text[96];
};
DrawState gState[kStateLabels];
int gStateOrder[kStateLabels];   // the labels with a draw state, in the order they were first named
int gStateCount;

// Which rectangle of which window a rectangle is: 10 * the window's label + 1 its own rectangle, 2 the original, 3 the previous,
// 4 the client rectangle, 5 the one at 0x34; -1 for any other. (The window is found from the rectangle's address and has to carry a
// label: every window made with operator new, and the world's windows, do.)
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

// The draw state of a draw object that has a label (it is made the first time it is named).
DrawState* StateOf(const void* object) {
    const int label = lab(object);
    if (label <= 0 || label >= kStateLabels) return nullptr;
    DrawState& state = gState[label];
    if (!state.used) {
        std::memset(&state, 0, sizeof state);
        state.used = true;
        state.shown = -1;
        gStateOrder[gStateCount++] = label;
    }
    return &state;
}

void RecordChange(InterfaceDrawObject* self, uint32_t flags, DOChange* change) {
    const int32_t label = lab(self);
    const bool rect = (flags & 0x10) != 0;
    ev("Change", {label, flags, (flags & 1) != 0 ? change->mFrame : 0, (flags & 2) != 0 ? change->mDepth : 0,
                  (flags & 4) != 0 ? change->mF08 : 0, rect ? RectCode(change->mRect) : 0, rect ? change->mRect->mLeft : 0,
                  rect ? change->mRect->mTop : 0, rect ? change->mRect->mRight : 0, rect ? change->mRect->mBottom : 0,
                  (flags & 0x40) != 0 ? change->mF0C : 0, (flags & 0x40) != 0 ? change->mF0E : 0});
    DrawState* state = StateOf(self);
    if (state != nullptr) ++state->changes;
    if ((flags & (0x10000 | 0xc00 | 0x40000 | 0x100)) == 0) return;
    const DOTextStyle* style = change->mText;
    char text[160];
    const char* shown = "";
    if ((flags & 0x100) != 0) {
        const char16_t* wide = style->mText;
        if (wide == nullptr) {
            shown = "(null)";
        } else {
            std::size_t n = 0;
            for (; wide[n] != 0 && n < 159; ++n) text[n] = wide[n] < 0x80 ? static_cast<char>(wide[n]) : '?';
            text[n] = 0;
            shown = text;
        }
    }
    ev("Change.style", {label, (flags & 0x10000) != 0 ? style->mF06[2] : 0, (flags & 0xc00) != 0 ? style->mF04 : 0,
                        (flags & 0xc00) != 0 ? style->mF05 : 0, (flags & 0x40000) != 0 ? style->mColour[0] : 0,
                        (flags & 0x40000) != 0 ? style->mColour[1] : 0, (flags & 0x40000) != 0 ? style->mColour[2] : 0,
                        (flags & 0x40000) != 0 ? style->mF13 : 0},
       {shown});
    if (state == nullptr) return;
    if ((flags & 0x10000) != 0) state->font = style->mF06[2];
    if ((flags & 0xc00) != 0) {
        state->just[0] = style->mF04;
        state->just[1] = style->mF05;
    }
    if ((flags & 0x40000) != 0) {
        state->colour[0] = style->mColour[0];
        state->colour[1] = style->mColour[1];
        state->colour[2] = style->mColour[2];
        state->colour[3] = style->mF13;
    }
    if ((flags & 0x100) != 0) {
        state->hasText = style->mText == nullptr ? 2 : 1;
        std::strncpy(state->text, shown, sizeof state->text - 1);
        state->text[sizeof state->text - 1] = 0;
    }
}

void RecordShow(InterfaceDrawObject* self, int16_t shown) {
    ev("Draw.Show", {lab(self), shown});
    DrawState* state = StateOf(self);
    if (state != nullptr) state->shown = shown;
}

// The draw objects of the draw object library (one per hash, named when first asked for) and of the pool the tests give windows.
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    uint32_t mHash = 0;
};
FakeDraw gDraws[kMaxDraws];
int gDrawCount;
FakeDraw gPool[kPoolDraws];
}  // namespace

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t hash) {
    ev("FindDrawObject", {static_cast<int64_t>(hash)});
    for (int i = 0; i < gDrawCount; ++i)
        if (gDraws[i].mHash == hash) return &gDraws[i];
    if (gDrawCount >= kMaxDraws) return nullptr;
    FakeDraw* draw = &gDraws[gDrawCount++];
    draw->mHash = hash;
    newlab(draw);
    StateOf(draw);
    return draw;
}

InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) { RecordChange(this, flags, change); }
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t shown) { RecordShow(this, shown); }
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// ---- the sprite draw object (w30b), the base of the text objects: never used here, but the vtables need its members
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t flags, DOChange* change) { RecordChange(this, flags, change); }
void InterfaceDrawObjectSprite::Show(int16_t shown) { RecordShow(this, shown); }
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}

// ---- the text objects (made by `new`; the title object's constructor records and gives the object its label). The original's hook
// replaces the constructor whole, so the base classes' constructors record nothing here.
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) { RecordChange(this, flags, change); }
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(data) {
    ev("TitleText.ctor", {lab(this), data != nullptr});
    StateOf(this);
}
InterfaceTitleTextObject::~InterfaceTitleTextObject() {}
void InterfaceTitleTextObject::UpdateTextures(char16_t*) {}

// ---- the members those classes own by value (the engine's sprite surface and list of w33b, the ltms text formatter and font
// renderer of w35a): never used here, but their constructors and destructors must exist to link. None logs: the original's hooks replace
// the text objects' constructors whole, so none of this runs there either.
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

// ---- the font table: a pool of 13 fonts, NULL for a number past it
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return index < static_cast<uint32_t>(kFonts) ? &gFonts[index] : nullptr;
}

// ---- the user information: the constructor records (the allocation gave the object its label); the names and the easy-mode answers are the world's
CFSUserInfo::CFSUserInfo() { ev("UserInfo.ctor", {lab(this)}); }
int32_t CFSUserInfo::GetNumValidGameLoginSlots() {
    ev("UserInfo.GetNumValidGameLoginSlots", {lab(this)});
    int32_t count = 0;
    for (int i = 0; i < kNames; ++i)
        if (gNameSet[i]) ++count;
    return count;
}
const TbStringBase<char16_t>* CFSUserInfo::GetGameLoginSlotUserName(EFSUserInfoGameUser slot) {
    ev("UserInfo.GetGameLoginSlotUserName", {lab(this), slot});
    return gNameSet[slot] ? &gNameViews[slot] : nullptr;
}
int32_t CFSUserInfo::IsGameLoginSlotUserEasyMode(EFSUserInfoGameUser slot) {
    ev("UserInfo.IsGameLoginSlotUserEasyMode", {lab(this), slot});
    return gEasy[slot];
}

// ---- the advisor and the islands
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) { ev("Advisor.Add", {lab(this), response.mId, clearFirst}); }
void CLBScreenComponentIslands::ResetIslandLoopingSounds() { ev("Islands.ResetIslandLoopingSounds", {lab(this)}); }

// ---- the string table and the converter: GetConstString answers the world's text for the number (NULL for a number it has none for),
// ConvertString widens it into the buffer
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    for (const StringEntry& entry : gStrings)
        if (entry.present && entry.id == static_cast<int32_t>(id)) return entry.bytes;
    return nullptr;
}
TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    ev("ConvertString", {lab(this), lab(out), size}, {reinterpret_cast<const char*>(source)});
    for (uint16_t i = 0; i < size; ++i) {
        out[i] = source[i];
        if (source[i] == 0) break;
    }
    return TbError(0);
}

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
    o.put(lab(w->mDrawObject));
    o.put(lab(w->mClientDrawObject));
    o.put(CallbackCode(w->mCallback));
    o.put(CallbackCode(w->mDefaultCallback));
    o.put(w->mCursor);
    o.put(lab(w->mParent));
    o.put(lab(w->mFont));
    for (int i = 0; i < 16; ++i) o.put(static_cast<int32_t>(reinterpret_cast<intptr_t>(w->mExtraParams[i])));
    if (kind == kUIKindButton) {
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
    }
}

void DumpTree(InterfaceWindow* w, int depth, Out& o) {
    for (; w != nullptr; w = w->mNext) {
        DumpWindow(w, depth, o);
        DumpTree(w->mChild, depth + 1, o);
    }
}

// The window an operation names: 0 to 3 a test window, 4 the lobby's, 5 the root, 100 + n the first window (depth first, from the
// root) with the n-th id of the Python module's table of the panel's ids; NULL for any other number.
constexpr int32_t kPanelIds[] = {0x7a13, 0x7a14, 0x7a15, 0x7a16, 0x7a17, 0x7a18, 0x7a19, 0x7a1a, 0x7a1b, 0x7a1c};
InterfaceWindow* Find(InterfaceWindow* w, int32_t id) {
    for (; w != nullptr; w = w->mNext) {
        if (w->mId == id) return w;
        InterfaceWindow* found = Find(w->mChild, id);
        if (found != nullptr) return found;
    }
    return nullptr;
}
InterfaceWindow* Window(int64_t spec) {
    if (spec >= 0 && spec < kTestWindows) return gTest[spec];
    if (spec == 4) return gLobbyWindow;
    if (spec == 5) return gRoot;
    if (spec >= 100 && spec < 100 + static_cast<int64_t>(sizeof kPanelIds / sizeof kPanelIds[0])) return Find(gRoot, kPanelIds[spec - 100]);
    return nullptr;
}
char16_t* Text(int64_t index) { return index >= 0 && index < kTexts && gTextSet[index] ? gTexts[index] : nullptr; }

// A key table's entries as the Python module makes them from a seed.
constexpr int16_t kStates[5] = {0, 1, -1, 0x7fff, 0x1234};
void FillTable(int table, int16_t count, int16_t enabled, int32_t seed) {
    for (int i = 0; i < kEntries; ++i) {
        CKeyTableEntry& entry = gEntries[table][i];
        entry.mId = static_cast<int16_t>(seed + i);
        entry.mKey = static_cast<int16_t>(seed * 3 + i);
        entry.mModifiers = static_cast<int16_t>(seed & 0xff);
        entry.mState = kStates[(seed + i) % 5];
        for (int k = 0; k < 8; ++k) entry.mF08[k] = static_cast<uint8_t>(seed + i + k);
        entry.mAction = i % 3 == 0 ? reinterpret_cast<void*>(static_cast<intptr_t>(0x1000 + i)) : nullptr;
    }
    CKeyTable& t = gTables[table];
    t.mEntries = gEntries[table];
    t.mCount = count;
    t.mEnabled = enabled;
}

void* ExtraValue(int64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(v))); }
}  // namespace

extern "C" {

// Clears the world's state (before the call log is set up: the labels come after, in tpw_lp_world).
void tpw_lp_reset() {
    gObserve = false;
    gDrawCount = 0;
    for (FakeDraw& draw : gDraws) {
        draw.mHash = 0;
        draw.mInLibrary = 0;
        draw.mResizable = 0;
        draw.mShown = 0;
    }
    for (FakeDraw& draw : gPool) {
        draw.mInLibrary = 0;
        draw.mResizable = 0;
        draw.mShown = 0;
    }
    std::memset(gState, 0, sizeof gState);
    gStateCount = 0;
    gUIKeyboardWindow = nullptr;
    gUIScriptPosition = 0;
    gUILastWindow = nullptr;
    gUIHebrew = false;
    gUIAutoRedraw = 0;
    gLoginPanelWindow = nullptr;
    gLoginPanelFirstPlayer = 0;
    gUserInfo = nullptr;
    gLobby = nullptr;
    gLobbyScreens = nullptr;
    gUIRootWindow = nullptr;
    for (int i = 0; i < kNames; ++i) {
        gNameSet[i] = false;
        gEasy[i] = 0;
    }
    for (int i = 0; i < kFonts; ++i) gFonts[i].SetID(i);
    for (StringEntry& entry : gStrings) entry.present = false;
    for (int i = 0; i < kTexts; ++i) gTextSet[i] = false;
    for (int i = 0; i < kMultibyte; ++i) gMultibyteSet[i] = false;
    std::memset(gText.mResourcePointers, 0, sizeof gText.mResourcePointers);
    gText.mResourcePointers[0] = gConverterBlock;
    gText.mResourcePointers[1] = gConverterBlock;
    gText.mResourcePointers[6] = gTableBlock;
    for (int i = 0; i < kBufferWords; ++i) gUIMainConvertBuffer[i] = u'#';
    std::memset(gHeap, 0, sizeof gHeap);
    if (!gHaveScriptWord) {
        gScriptFirstWord = gLoginPanelWindowData[0];
        gHaveScriptWord = true;
    }
    gLoginPanelWindowData[0] = gScriptFirstWord;
    for (int i = 0; i < kTables; ++i) FillTable(i, 3, 1, 10 * i + 1);
    for (int i = 0; i < kTables; ++i) *gTableGlobals[i] = &gTables[i];
}

// Registers a wide text (the Python module's table, the same on both sides) and a multibyte text of the string table.
void tpw_lp_text(int32_t index, const char16_t* text, int32_t length) {
    for (int i = 0; i < length && i < kTextUnits - 1; ++i) gTexts[index][i] = text[i];
    gTexts[index][length < kTextUnits - 1 ? length : kTextUnits - 1] = 0;
    gTextSet[index] = true;
}
void tpw_lp_multibyte(int32_t index, const unsigned char* text, int32_t length) {
    for (int i = 0; i < length && i < 63; ++i) gMultibyte[index][i] = text[i];
    gMultibyte[index][length < 63 ? length : 63] = 0;
    gMultibyteSet[index] = true;
}

// Builds the world's windows and names its fixed objects for the call log (the log is on: the window constructors record). A test
// window's rectangle and id are made from its number.
void tpw_lp_world() {
    tpw_cl_label(kLobbyLabel, gLobbyBlock);
    tpw_cl_label(kScreensLabel, gScreensBlock);
    tpw_cl_label(kThemeLabel, gThemeBlock);
    tpw_cl_label(kAdvisorLabel, gAdvisorBlock);
    tpw_cl_label(kUserInfoLabel, gUserInfoBlock);
    tpw_cl_label(kTableLabel, gTableBlock);
    tpw_cl_label(kConverterLabel, gConverterBlock);
    tpw_cl_label(kBufferLabel, gUIMainConvertBuffer);
    for (int i = 0; i < kFonts; ++i) tpw_cl_label(kFontLabel + i, &gFonts[i]);
    for (int i = 0; i < kPoolDraws; ++i) {
        tpw_cl_label(kPoolDrawLabel + i, &gPool[i]);
        StateOf(&gPool[i]);
    }
    for (auto& block : gWindowBlocks) std::memset(block, 0xa5, kWindowBlock);
    tpw_cl_label(kRootLabel, gWindowBlocks[0]);
    gRoot = new (gWindowBlocks[0]) InterfaceWindow(nullptr, 1, 0x1000, 0, 0, 2048, 1536);
    tpw_cl_label(kLobbyWindowLabel, gWindowBlocks[1]);
    gLobbyWindow = new (gWindowBlocks[1]) InterfaceWindow(gRoot, 1, 0xbf431, 0, 0, 2048, 1536);
    gRoot->mChild = gLobbyWindow;   // (the constructor does not link the window to its parent)
    for (int i = 0; i < kTestWindows; ++i) {
        tpw_cl_label(kTestLabel + i, gWindowBlocks[2 + i]);
        gTest[i] = new (gWindowBlocks[2 + i]) InterfaceWindow(nullptr, 1, 0x2001 + i, static_cast<int16_t>(10 * i),
                                                               static_cast<int16_t>(20 * i), static_cast<int16_t>(100 + 10 * i),
                                                               static_cast<int16_t>(200 + 20 * i));
    }
    gUIRootWindow = gRoot;
    std::memset(gLobbyBlock, 0, sizeof gLobbyBlock);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyBlock);
    gLobby->mWindow = gLobbyWindow;
    std::memset(gScreensBlock, 0, sizeof gScreensBlock);
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(gScreensBlock);
    gLobbyScreens->mThemeIslands = reinterpret_cast<CLBScreenComponentThemeIslands*>(gThemeBlock);
    gLobbyScreens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(gAdvisorBlock);
}

// One operation; a[] are its arguments, out[0..3] the answer. The numbers are those of the Python module.
void tpw_lp_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = 0;
    const int32_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    gObserve = true;
    InterfaceWindow* w = Window(a[0]);
    switch (op) {
    case 1:
        CreateLoginPanel();
        break;
    case 2:
        FillLoginNames();
        break;
    case 3:
        UI_TITLE_COLOUR(w);
        break;
    case 4:   // SetFont: window, font (-1 none)
        w->SetFont(a[1] >= 0 && a[1] < kFonts ? &gFonts[a[1]] : nullptr);
        break;
    case 5:
        w->SetFontJust(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
        break;
    case 6:
        w->SetFontColour(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
        break;
    case 7: {   // setExtraParam: window, index, value. The draw objects are put back after the call (a negative index overwrites them).
        InterfaceDrawObject* draw = w->mDrawObject;
        InterfaceDrawObject* client = w->mClientDrawObject;
        out[0] = w->setExtraParam(static_cast<int16_t>(a[1]), ExtraValue(a[2]));
        out[1] = reinterpret_cast<intptr_t>(w->mDrawObject) == reinterpret_cast<intptr_t>(draw) ? 0 : static_cast<int32_t>(reinterpret_cast<intptr_t>(w->mDrawObject));
        out[2] = reinterpret_cast<intptr_t>(w->mClientDrawObject) == reinterpret_cast<intptr_t>(client)
                     ? 0
                     : static_cast<int32_t>(reinterpret_cast<intptr_t>(w->mClientDrawObject));
        w->mDrawObject = draw;
        w->mClientDrawObject = client;
        break;
    }
    case 8:
        w->SetClientDrawText(Text(a[1]));
        break;
    case 9:
        out[0] = w->SetText(Text(a[1]));
        break;
    case 10: {   // UIMain_GetString
        const char16_t* result = UIMain_GetString(static_cast<int32_t>(a[0]));
        out[0] = result == nullptr ? 0 : result == gUIMainConvertBuffer ? 1 : 2;
        break;
    }
    case 11:
        DisableAllKeytables();
        break;
    case 20:   // a player's name: slot, text (-1 none)
        if (a[1] >= 0 && Text(a[1]) != nullptr) {
            char16_t* text = Text(a[1]);
            std::size_t length = 0;
            while (text[length] != 0) ++length;
            new (&gNameViews[a[0]]) TbStringBase<char16_t>(text, static_cast<uint32_t>(length));
            gNameSet[a[0]] = true;
        } else {
            gNameSet[a[0]] = false;
        }
        break;
    case 21:
        gEasy[a[0]] = static_cast<int32_t>(a[1]);
        break;
    case 22:   // the user information: 0 none, 1 the world's block
        gUserInfo = a[0] != 0 ? reinterpret_cast<CFSUserInfo*>(gUserInfoBlock) : nullptr;
        break;
    case 23: {   // a string of the table: number, multibyte text (-1 none)
        StringEntry* entry = nullptr;
        for (StringEntry& e : gStrings)
            if (e.present && e.id == static_cast<int32_t>(a[0])) entry = &e;
        if (entry == nullptr)
            for (StringEntry& e : gStrings)
                if (!e.present && entry == nullptr) entry = &e;
        if (entry != nullptr) {
            entry->present = a[1] >= 0 && a[1] < kMultibyte && gMultibyteSet[a[1]];
            entry->id = static_cast<int32_t>(a[0]);
            if (entry->present) std::memcpy(entry->bytes, gMultibyte[a[1]], 64);
        }
        break;
    }
    case 24:   // the converter: 0 not loaded (the Unicode-to-multibyte pointer is NULL), 1 loaded
        gText.mResourcePointers[1] = a[0] != 0 ? gConverterBlock : nullptr;
        break;
    case 25:   // a key table: index, count, enabled, seed
        FillTable(static_cast<int>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int32_t>(a[3]));
        break;
    case 26:   // a font's id
        gFonts[a[0]].SetID(static_cast<int32_t>(a[1]));
        break;
    case 27:   // a window's draw objects (pool numbers, -1 none): a raw store
        w->mDrawObject = a[1] >= 0 && a[1] < kPoolDraws ? &gPool[a[1]] : nullptr;
        w->mClientDrawObject = a[2] >= 0 && a[2] < kPoolDraws ? &gPool[a[2]] : nullptr;
        break;
    case 28:   // the script's first word (a raw store)
        gLoginPanelWindowData[0] = static_cast<int16_t>(a[0]);
        break;
    case 29:   // the panel's global: a window (the same numbers) or none
        gLoginPanelWindow = w;
        break;
    case 30:   // the converted-text buffer's first characters
        for (int i = 0; i < kBufferWords; ++i) gUIMainConvertBuffer[i] = static_cast<char16_t>(a[0] + i);
        break;
    default:
        break;
    }
    gObserve = false;
    for (int i = 0; i < 4; ++i) gHeap[i] += tpw_cl_heap[i] - heapBefore[i];
}

// The state: the panel's globals, the windows, the draw objects, the key tables, the converted-text buffer and the heap traffic.
// Answers the number of words.
int32_t tpw_lp_state(int64_t* out) {
    Out o{out};
    o.put(lab(gLoginPanelWindow));
    o.put(gLoginPanelFirstPlayer);
    o.put(lab(gUserInfo));
    o.put(lab(gUIKeyboardWindow));
    o.put(gUIScriptPosition);
    o.put(lab(gUILastWindow));
    // the root's tree (the lobby's window, the panels) and then the test windows alone
    Out tree{out + o.n + 1};
    DumpTree(gRoot, 0, tree);
    o.put(tree.n);
    o.p += tree.n;
    o.n += tree.n;
    for (int i = 0; i < kTestWindows; ++i) DumpWindow(gTest[i], 0, o);
    // the draw objects, in the order they were named: label, shown, changes, font, justification, colour, text, and the object's own words
    o.put(gStateCount);
    for (int i = 0; i < gStateCount; ++i) {
        const int label = gStateOrder[i];
        const DrawState& s = gState[label];
        o.put(label);
        o.put(s.shown);
        o.put(s.changes);
        o.put(s.font);
        o.put(s.just[0]);
        o.put(s.just[1]);
        for (int k = 0; k < 4; ++k) o.put(s.colour[k]);
        o.put(s.hasText);
        std::size_t length = std::strlen(s.text);
        o.put(static_cast<int64_t>(length));
        for (std::size_t k = 0; k < length; ++k) o.put(static_cast<unsigned char>(s.text[k]));
    }
    // the draw object library's objects: hash and the counts the window setters keep in the object
    o.put(gDrawCount);
    for (int i = 0; i < gDrawCount; ++i) {
        o.put(gDraws[i].mHash);
        o.put(gDraws[i].mInLibrary);
        o.put(gDraws[i].mResizable);
        o.put(gDraws[i].mShown);
    }
    for (int i = 0; i < kPoolDraws; ++i) {
        o.put(gPool[i].mInLibrary);
        o.put(gPool[i].mResizable);
        o.put(gPool[i].mShown);
    }
    // the key tables: the table's own words and all 16 entries
    for (int t = 0; t < kTables; ++t) {
        const CKeyTable& table = gTables[t];
        o.put(*gTableGlobals[t] == &gTables[t]);
        o.put(table.mEnabled);
        o.put(table.mCount);
        for (int i = 0; i < kEntries; ++i) {
            const CKeyTableEntry& e = table.mEntries[i];
            o.put(e.mId);
            o.put(e.mKey);
            o.put(e.mModifiers);
            o.put(e.mState);
            for (int k = 0; k < 8; ++k) o.put(e.mF08[k]);
            o.put(e.mAction != nullptr);
        }
    }
    for (int i = 0; i < kBufferWords; ++i) o.put(gUIMainConvertBuffer[i]);
    for (int i = 0; i < 4; ++i) o.put(gHeap[i]);
    o.put(tpw_cl_n);
    return static_cast<int32_t>(o.n);
}

}  // extern "C"
