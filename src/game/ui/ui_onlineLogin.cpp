// ui_onlineLogin.cpp: name proven by __sinit_ui_onlineLogin_cpp in the main application.

#include "AdvisorScoring.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/macdoze.h"
#include "std_basic_ifstream.h"
#include "TbDynamicStringTemplate.h"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// The online city info overlay of the island lobby (0x101e1f64, 0x101e20f0) and the online login panel's message function (0x101e24c8).

int32_t gOnlineLoginPanelFlag = 0;                        // 0x10395d2c
char16_t gOnlineLoginPassword[33] = {};                   // 0x10395d30
char16_t gOnlineLoginName[33] = {};                       // 0x10395d72
// 0x101e24c8: the login panel's message function.
int32_t OnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    int32_t result = 0;
    (void)b;
    if (message == 0x100) {
        if (a == -1) {
            if (gOnlineSystem == nullptr) {
                void* memory = online_callee::OperatorNew(0x404);
                CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(memory);
                if (memory != nullptr) new (memory) CFSOnlineSystem();
                gOnlineSystem = system;
            }
            gOnlineSystem->DoLogin(gOnlineLoginName, gOnlineLoginPassword);
        } else if (a == -2) {
            RemoveOnlineLoginPanel(1);
            result = 1;
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            UI_SendMessage(gLobby->mWindow, 6, 0, 0);
            UIMain_DisableAllKeyTables();
        } else if (message == 0x14) {
            gOnlineLoginPanelFlag = 0;
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
            UIMain_EnableAllKeyTables();
        }
    } else if (message == 0x803) {
        if (a == 0x49)
            window->GetWindow(0x4b)->CaptureKb(0);
        else
            window->GetWindow(0x49)->CaptureKb(0);
    } else if (message < 0x803) {
        if (message >= 0x802) {
            if (a == 0x49)
                window->GetWindow(0x4b)->CaptureKb(0);
            else
                UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x805) {
        RemoveOnlineLoginPanel(1);
        result = 1;
    }
    return result;
}

// Unit v5981: InterfaceWindow::link (0x101ddf20), the island lobby controller's static initialiser (0x101e1e3c), the online login panel's
// message function (0x101e26fc), the outbox address panel's message function (0x101e4b00) and the JPEG loader (0x101e538c).

// 0x101e26fc: the online login panel's handler first; the default dialog handler when it did not take the message.
int32_t LobbyOnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (OnlineLoginPanelDlgMsg(window, message, a, b) != 0) return 0;
    return DefaultDialogCallback(window, message, a, b);
}

// CreateOnlineLoginPanel (0x101e27bc): makes the login dialog and fits its name and password edit boxes and the labels and title.

int16_t gOnlineLoginPanelScript[2] = {0, 1};     // 0x102b02fc
char16_t gOnlineLoginEmptyText[2] = {0, 0x75};   // 0x1024ee76
void (*gOnlineLoginFailureFn)() = nullptr;       // 0x10395d28

void CreateOnlineLoginPanel(void (*onFailure)()) {
    InterfaceWindow* dialog = Dialog(gOnlineLoginPanelScript, OnlineLoginPanelDlgMsg, 0);
    gOnlineLoginPanelFlag = static_cast<int32_t>(reinterpret_cast<uintptr_t>(dialog));
    gOnlineLoginFailureFn = onFailure;
    // The Mac reloads the dialog from the flag (0x10395d2c) for the three direct children; nothing changes the flag in between, and a host
    // pointer does not fit the 32-bit flag, so the local is used.
    auto PanelWindow = [dialog]() { return dialog; };

    // ---- the name edit box (0x49) and its label (0x4a)
    InterfaceEditBox* nameBox = static_cast<InterfaceEditBox*>(PanelWindow()->GetWindow(0x49));
    nameBox->SetStaticTextArea(gOnlineLoginName, 0x10);
    void* memory = lender_callee::OperatorNew(0x19c);
    InterfaceCursorPosTextObjectSprite* cursorSprite = static_cast<InterfaceCursorPosTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceCursorPosTextObjectSprite(nullptr);
    nameBox->SetClientDrawObject(cursorSprite);
    nameBox->SetFont(GetFontTableEntry(7));
    nameBox->SetFontColour(0xff, 0xff, 0xff, 0xff);
    nameBox->SetFontJust(0, 0);
    nameBox->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    nameBox->SetText(gOnlineLoginEmptyText);

    memory = lender_callee::OperatorNew(0x19c);
    InterfaceTextObjectSprite* labelSprite = static_cast<InterfaceTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceTextObjectSprite(nullptr);
    nameBox->GetWindow(0x4a)->SetClientDrawObject(labelSprite);
    InterfaceFont* font = GetFontTableEntry(7);
    nameBox->GetWindow(0x4a)->SetFont(font);
    char16_t* text = const_cast<char16_t*>(UIMain_GetString(0xed));
    nameBox->GetWindow(0x4a)->SetClientDrawText(text);

    // ---- the password edit box (0x4b) and its label (0x4c)
    InterfaceEditBox* passBox = static_cast<InterfaceEditBox*>(PanelWindow()->GetWindow(0x4b));
    passBox->SetStaticTextArea(gOnlineLoginPassword, 0x10);
    memory = lender_callee::OperatorNew(0x19c);
    InterfacePasswordTextObject* passSprite = static_cast<InterfacePasswordTextObject*>(memory);
    if (memory != nullptr) new (memory) InterfacePasswordTextObject(nullptr);
    passBox->SetClientDrawObject(passSprite);
    passBox->SetFont(GetFontTableEntry(7));
    passBox->SetFontColour(0xff, 0xff, 0xff, 0xff);
    passBox->SetFontJust(0, 0);
    passBox->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    passBox->SetText(gOnlineLoginEmptyText);

    memory = lender_callee::OperatorNew(0x19c);
    labelSprite = static_cast<InterfaceTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceTextObjectSprite(nullptr);
    passBox->GetWindow(0x4c)->SetClientDrawObject(labelSprite);
    font = GetFontTableEntry(7);
    passBox->GetWindow(0x4c)->SetFont(font);
    text = const_cast<char16_t*>(UIMain_GetString(0xee));
    passBox->GetWindow(0x4c)->SetClientDrawText(text);
    nameBox->CaptureKb(0);

    // ---- the title (0x4d)
    InterfaceWindow* title = PanelWindow()->GetWindow(0x4d);
    memory = lender_callee::OperatorNew(0x19c);
    InterfaceTitleTextObject* titleSprite = static_cast<InterfaceTitleTextObject*>(memory);
    if (memory != nullptr) new (memory) InterfaceTitleTextObject(nullptr);
    title->SetClientDrawObject(titleSprite);
    title->SetFont(GetFontTableEntry(5));
    title->SetFontJust(1, 1);
    title->SetText(const_cast<char16_t*>(UIMain_GetString(0xec)));
    UI_TITLE_COLOUR(title);
}

// NewsPanelMsg (0x101e2f84) and RemoveOnlineLoginPanel (0x101e2be4).

// 0x10395d28: a pointer to a function descriptor (called through __ptr_glue), NULL until set; original name unknown.
void (*gOnlineLoginPanelHook)() = nullptr;

void RemoveOnlineLoginPanel(int32_t arg) {
    UI_SendMessage(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(gOnlineLoginPanelFlag))), 4, 0, 0);
    if (arg != 0) {
        if (gOnlineLoginPanelHook != nullptr) gOnlineLoginPanelHook();
    }
}

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

uint8_t gOnlineLoginStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395db4
uint8_t gOnlineLoginNodeA[16];                                                                        // 0x10395dc8
// 0x101e2c78, 64 bytes.
void __sinit_ui_onlineLogin_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineLoginStringA, gOnlineLoginEmptyText + 1);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineLoginNodeA);
}
