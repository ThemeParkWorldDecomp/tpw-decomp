// ui_onlineNewsPanel.cpp: name proven by __sinit_ui_onlineNewsPanel_cpp in the main application.

#include "AdvisorScoring.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "WindowScripts.inc"
#include <cstdint>

// Unit v5225: the online news panel's text window (src/ui_onlineNewsPanel.cpp in the original).
//   TextPanelMsg         0x101e30b4 (376): the message function of the news text window
//   CreateOnlineNewsPanel 0x101e3264 (820): makes the panel once the lobby runs
//   CalcScrollBar        0x101e2ce8 (632): shows the scroll bar when the text is taller than the window

// The panel's globals (original names unknown).
uint32_t gOnlineNewsFont = 8;                  // 0x102b05c0  the font index of the text window
int16_t gOnlineNewsScript[] = {TPW_WINDOW_SCRIPT_102B0440};   // 0x102b0440  the dialog script (the words are the image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc))
uint8_t gOnlineNewsShowInbox = 0;              // 0x10395dd4  1: the first text of the online system, 0: the second
InterfaceWindow* gOnlineNewsPanel = nullptr;   // 0x10395dd8  the panel, NULL until made

int32_t NewsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e2f84 (another unit)
int32_t TextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
void CalcScrollBar();
void CreateOnlineNewsPanel();

namespace {

CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    return gOnlineSystem;
}

// The text shown: the online system's string at 0xa0 (flag set) or at 0xbc.
char16_t* NewsText() {
    if (gOnlineNewsShowInbox != 0) return OnlineSystem()->mF98.mData;
    return OnlineSystem()->mFB4.mData;
}

}  // namespace

// Shows the scroll bar when the text, measured in the window's width, is taller than the window.
void CalcScrollBar() {
    InterfaceWindow* textWindow = gOnlineNewsPanel->GetWindow(0x5b7f);
    InterfaceWindow* bar = textWindow->GetWindow(0x5b80);
    CoolRect client;
    char16_t* text = NewsText();
    textWindow->GetClientRect(&client);

    int32_t top = client.mTop;
    int32_t bottom = client.mBottom;
    int32_t left = client.mLeft;
    int32_t right = client.mRight;
    int32_t heightProduct = static_cast<int32_t>(gUIRealScreenHeight) * (bottom - top);
    int32_t quotient = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * heightProduct) >> 32);
    quotient >>= 8;
    int32_t limitBottom = quotient + static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    int32_t widthProduct = static_cast<int32_t>(gUIRealScreenWidth) * (right - left);
    int32_t limitRight = (widthProduct >> 11) + ((widthProduct < 0 && (widthProduct & 0x7ff) != 0) ? 1 : 0);

    TbRect limit;
    limit.left = 0;
    limit.top = 0;
    limit.right = limitRight;
    limit.bottom = limitBottom;
    TbRect size;
    size.left = 0;
    size.top = 0;
    size.right = 0;
    size.bottom = 0;
    UI_MeasureText(text, limit, static_cast<int8_t>(gOnlineNewsFont), 0, 0, 1, &size);

    int32_t measured = (size.bottom * 0x600) / gUIRealScreenHeight;
    int32_t windowHeight = bottom - top;
    int16_t fontHeight = static_cast<int16_t>(GetFontTableEntry(gOnlineNewsFont)->GetHeight());
    int32_t step = (static_cast<int32_t>(fontHeight) * 0x600) / gUIRealScreenHeight;
    if (measured > windowHeight) {
        bar->Show(1);
        static_cast<InterfaceScrollBar*>(bar)->SetMinMax((measured - windowHeight) / step, 0);
    } else {
        bar->Show(0);
    }
    if (static_cast<int16_t>(bar->SetValue(0)) == 0) textWindow->SetText(text);
}

// NewsPanelMsg (0x101e2f84) and RemoveOnlineLoginPanel (0x101e2be4).

extern InterfaceWindow* gOnlineNewsPanel;   // 0x10395dd8
void RemoveOnlineNewsPanel();               // 0x101e35c4

// The news panel's messages: 0x14 and 0x15 pass the lobby window a message 6 (1 / 0), 0x100 with -1 removes the panel.
int32_t NewsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        UI_SendMessage(gLobby->mWindow, 6, 0, 0);
    } else if (message < 0x15) {
        if (message >= 0x14) {
            gOnlineNewsPanel = nullptr;
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x100 && a == -1) {
        RemoveOnlineNewsPanel();
        return 0;
    }
    return DefaultDialogCallback(window, message, a, b);
}

// The text window's messages: 0x800 scrolls the text, a key (0x101) with 1 picks which text, the default handler has the rest.
int32_t TextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x800) {
        int16_t fontHeight = static_cast<int16_t>(GetFontTableEntry(gOnlineNewsFont)->GetHeight());
        int32_t product = b * static_cast<int32_t>(fontHeight);
        product *= 0x600;
        int32_t offset = product / static_cast<int32_t>(gUIRealScreenHeight);
        window->SetTextOffset(0, -offset);
        window->SetText(NewsText());
    } else if (message == 0x101 && b == 1) {
        if (a == 0x5b82) gOnlineNewsShowInbox = 1;
        else gOnlineNewsShowInbox = 0;
        CalcScrollBar();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void CreateOnlineNewsPanel() {
    if (gOnlineNewsPanel != nullptr) return;
    if (gLobby == nullptr) return;
    gOnlineNewsPanel = Dialog(gOnlineNewsScript, NewsPanelMsg, 0);
    gOnlineNewsPanel->setZ(static_cast<int16_t>(gOnlineNewsPanel->mZ + 4));
    InterfaceWindow* text = gOnlineNewsPanel->GetWindow(0x5b7f);
    text->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    text->SetFont(GetFontTableEntry(gOnlineNewsFont));
    text->SetClientDrawText(nullptr);
    text->SetFontWrap(1, 0);
    text->SetFontJust(0, 0);
    gOnlineNewsShowInbox = 1;
    CalcScrollBar();
    text->SetCallbackFunction(TextPanelMsg);
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(text->GetWindow(0x5b81));
    buttons->SetCurrent(0x5b82);

    InterfaceWindow* label = buttons->GetWindow(0x5b82);
    label->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    label->SetFontJust(1, 1);
    label->SetFont(GetFontTableEntry(6));
    label->SetFontColour(0, 0, 0, 0xff);
    label->SetClientDrawText(nullptr);
    label->SetText(const_cast<char16_t*>(UIMain_GetString(0x10c)));

    label = buttons->GetWindow(0x5b83);
    label->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    label->SetFontJust(1, 1);
    label->SetFont(GetFontTableEntry(6));
    label->SetFontColour(0, 0, 0, 0xff);
    label->SetClientDrawText(nullptr);
    label->SetText(const_cast<char16_t*>(UIMain_GetString(0x10d)));

    UIMain_SetTitleText(gOnlineNewsPanel, 0x5b84, const_cast<char16_t*>(UIMain_GetString(0x10b)));
    gOnlineNewsPanel->CaptureKb(0);
}

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

extern const char16_t gOnlineNewsText[];
alignas(16) const char16_t gOnlineNewsText[] = u"upload";   // 0x1024ee86: the literal given to the news string
uint8_t gOnlineNewsStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395ddc
uint8_t gOnlineNewsNodeA[16];                                                                         // 0x10395df0
// 0x101e35c4, 56 bytes.
void RemoveOnlineNewsPanel() { UI_SendMessage(gOnlineNewsPanel, 4, 0, 0); }
// 0x101e3628, 60 bytes.
void __sinit_ui_onlineNewsPanel_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineNewsStringA, gOnlineNewsText);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineNewsNodeA);
}
