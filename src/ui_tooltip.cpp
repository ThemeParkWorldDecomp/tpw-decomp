// The tooltip window (main app, ui_tooltip.cpp, 0x1018be38..0x1018c8ec, unit w56): CUIHelpWindow, the one subclass of InterfaceHelpText, its
// static object, and the free functions the game calls (InitHelpText, SetHelpTextString, MoveHelpText).
//
// InitHelpText (at start-up) makes the text window: 0x23e to 0x5c2 across and as high as the tooltip font's text needs at the screen's
// real resolution, ending at 0x5f0, under the interface's root window, with a text draw object, the tooltip font (font 7 of the table),
// white text centred in both directions and wrapped, the "f_helpbg" background, itself as extra parameter 0, depth 0x26, and hidden.
// The pump then tells the object (the interface's help text handler) when the pointer moves into a window: ShowHelpText asks that window
// for its help text with message 0xc, takes the string from the answer or, when it has none, looks the number up in the interface's string
// table, and shows it. Everything is guarded by the flag at +8 (initialised, see the header) and, for the showing, by the enabled flag.
//
// The host's differences: message 0xc answers the ADDRESS of the pair (gUIHelpTextResult, then the string word at 0x10395c64), which the
// host's int32_t answer cannot carry (src/InterfaceWindow_draw.cpp answers its low word), so ShowHelpText reads the pair directly. A
// handler that answered some other pair would be misread, but the default handler and the thumb track's forward are the only ones.
#include "ui_tooltip.h"

#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

// 0x1038cd34: the tooltip object, and the Mac's 0x10395c64.
CUIHelpWindow gUIHelpWindow;
char16_t* gUIHelpTextString;

namespace {

// The Mac's wcschr and wcslen on 16-bit characters.
bool HasNewLine(const char16_t* text) {
    for (; *text != 0; ++text)
        if (*text == u'\n') return true;
    return false;
}

bool IsEmpty(const char16_t* text) { return *text == 0; }

// The empty string at 0x1024db9a, the text when the help text number is -1.
char16_t kEmptyText[] = {0};

// The justification SetFontJust is given for a text: centred both ways, or at the top when the text has more than one line.
void Justify(InterfaceWindow* window, const char16_t* text) { window->SetFontJust(1, HasNewLine(text) ? 0 : 1); }

}  // namespace

// 0x1018bf00 (104 bytes): the Mac puts this class's vtable back, destroys the base and, for a positive delete flag, frees the object.
CUIHelpWindow::~CUIHelpWindow() {}

// 0x1018bf94 (348 bytes): asks the window the pointer is in for its help text (message 0xc). The answer 0 or -1 (no window, a window
// that is being destroyed, a window without a handler) ends it. Otherwise the answer points at two words: the help text number and a
// string. The string is used when it is not NULL; else the number picks the string out of the interface's table (string table 7 of the game
// text object, at gText + 0x568, converted to Unicode), and -1 is the empty string. A text with a new line is shown at the top, any other
// centred; the base class notes the text is wanted, and an empty text hides the tooltip again.
void CUIHelpWindow::ShowHelpText() {
    if (mShowing == 0) return;
    if (mEnabled == 0) return;
    const int32_t answer = UI_SendMessage(mWindow, 0xc, 0, 0);
    if (answer == 0 || answer == -1) return;
    char16_t* text;
    if (gUIHelpTextString != nullptr) {
        text = gUIHelpTextString;
    } else if (gUIHelpTextResult == -1) {
        text = kEmptyText;
    } else {
        const TbITable* strings = static_cast<const TbITable*>(gText.mResourcePointers[7]);
        text = UIMain_ConvertMulti(strings->GetConstString(static_cast<uint32_t>(gUIHelpTextResult)));
    }
    Justify(mHelpWindow, text);
    mHelpWindow->SetClientDrawText(text);
    InterfaceHelpText::ShowHelpText();
    if (IsEmpty(text)) {
        HideHelpText();
    } else {
        mHelpWindow->Show(1);
    }
}

// 0x1018c124 (88 bytes)
void CUIHelpWindow::HideHelpText() {
    if (mShowing != 0) {
        if (mHelpWindow != nullptr) mHelpWindow->Show(0);
        InterfaceHelpText::HideHelpText();
    }
}

void CUIHelpWindow::OnFrame() {}   // 0x1018c1b0

// 0x1018c1e4 (84 bytes): the base class notes the window and the tick; then the text is shown (virtual).
void CUIHelpWindow::OnMouseMovedIn(InterfaceWindow* window) {
    if (mShowing != 0) {
        InterfaceHelpText::OnMouseMovedIn(window);
        ShowHelpText();
    }
}

// 0x1018c280 (452 bytes): makes the text window. Its height is the tooltip font's (font 7) height in the 1536-high interface at the real
// resolution (a signed division by the screen's height, which is not checked for 0), plus 10. QUIRK: the height is not cut to 16 bits
// before it is subtracted from 0x5f0, so the top and bottom are the 16-bit cuts of the 32-bit values.
void CUIHelpWindow::Init() {
    const int16_t fontHeight = static_cast<int16_t>(GetFontTableEntry(7)->GetHeight());
    const int32_t height = (fontHeight * 0xc00) / static_cast<int32_t>(gUIRealScreenHeight) + 10;
    const int32_t top = 0x5f0 - height;
    mHelpWindow = gUIRootWindow->CreateObject(1, 1, -0xcaa1c, 0x23e, static_cast<int16_t>(top), 0x5c2, static_cast<int16_t>(top + height));
    mHelpWindow->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    mHelpWindow->SetFontJust(1, 1);
    mHelpWindow->SetFontWrap(1, 1);
    mHelpWindow->SetFont(GetFontTableEntry(7));
    mHelpWindow->SetFontColour(0xff, 0xff, 0xff, 0xff);
    mHelpWindow->SetClientDrawText(nullptr);
    mHelpWindow->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("f_helpbg")));
    mHelpWindow->setExtraParam(0, this);
    mHelpWindow->setZ(0x26);
    mHelpWindow->Enable(0);
    mHelpWindow->SetCallbackFunction(HelpTextMsg);
    mShowing = 1;
}

// 0x1018c470 (16 bytes): forgets the window (it is being destroyed with its parent) and the initialised flag.
void CUIHelpWindow::DeInit() {
    mHelpWindow = nullptr;
    mShowing = 0;
}

// 0x1018c4ac and 0x1018c528 (80 bytes each): the base class's, then the window shown or hidden (no test for a missing window).
void CUIHelpWindow::Enable() {
    if (mShowing != 0) {
        InterfaceHelpText::Enable();
        mHelpWindow->Show(1);
    }
}

void CUIHelpWindow::Disable() {
    if (mShowing != 0) {
        InterfaceHelpText::Disable();
        mHelpWindow->Show(0);
    }
}

// 0x1018c5a8 (56 bytes): while enabled the text is worked out again (virtual).
void CUIHelpWindow::Update() {
    if (mEnabled != 0) ShowHelpText();
}

// 0x1018be38 (144 bytes)
int32_t HelpTextMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) static_cast<InterfaceHelpText*>(window->getExtraParam(0))->DeInit();
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x1018c60c (100 bytes). Init always leaves the flag set; the object is then disabled (the base class's Disable, not the virtual one) and
// its window hidden, until the settings enable the tooltips (UI_EnableHelpText).
void InitHelpText() {
    gUIHelpWindow.Init();
    if (gUIHelpWindow.mShowing != 0) {
        gUIHelpWindow.InterfaceHelpText::Disable();
        gUIHelpWindow.mHelpWindow->Show(0);
    }
    UI_SetHelpTextHandler(&gUIHelpWindow);
}

// 0x1018c694 (228 bytes). No test for a missing window.
void SetHelpTextString(char16_t* text) {
    if (gUIHelpWindow.mShowing != 0 && gUIHelpWindow.mEnabled != 0) {
        Justify(gUIHelpWindow.mHelpWindow, text);
        if (IsEmpty(text)) {
            gUIHelpWindow.HideHelpText();
        } else {
            gUIHelpWindow.mHelpWindow->Show(1);
            gUIHelpWindow.mHelpWindow->SetText(text);
        }
    }
}

// 0x1018c7a4 (292 bytes). The offsets are 32-bit differences that ShiftPosition takes as shorts.
void MoveHelpText(int16_t x, int16_t y) {
    InterfaceWindow* window = gUIHelpWindow.mHelpWindow;
    if (x == -1 && y == -1) {
        if (window != nullptr) {
            CoolRect rect;
            window->GetOriginalPosition(&rect);
            window->SetPosition(&rect);
        }
    } else if (window != nullptr) {
        CoolRect rect;
        window->GetPosition(&rect, 1);
        if (x == -1) x = rect.mLeft;
        if (y == -1) y = rect.mTop;
        window->ShiftPosition(static_cast<int16_t>(x - rect.mLeft), static_cast<int16_t>(y - rect.mTop));
    }
}
