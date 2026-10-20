// OnlineParkFindPanelMsg 0x101e93b0 (700 bytes) and CreateOnlinePanel 0x101e8f08 (916 bytes), Mac file ui_onlineParkFindPanel.cpp.
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_tooltip.h"

int16_t gOnlinePanelScript[2] = {0, 1};         // 0x102b0834
int16_t gOnlineUnderlayScriptA[2] = {0, 1};     // 0x102b0a22
int16_t gOnlineUnderlayScriptB[2] = {0, 1};     // 0x102b0b5e
uint32_t gOnlineEditFontIndex = 7;              // 0x102b0c50
uint8_t gOnlineByte78;                          // 0x10395e78
int32_t gOnlineWord88;                          // 0x10395e88
char16_t gOnlineEditText[256];                  // 0x10395e9c
char16_t gFindPanelPlayerName[64];              // 0x103960a4
void (*gFindPanelCloseCallback)();              // 0x103960c4
int32_t gFindPanelWord;                         // 0x103960c8

namespace {
const char kStopSearchText[] = "WL / MW : Stop the search and display whatever has been got in the search\n";   // 0x1024ef8d
}

// Builds the online panel (script 0x102b0834, message function OnlinePanelMsg) with its contacts and messages lists, the edit box and the
// two button windows, then the full screen window over the root.
void CreateOnlinePanel() {
    gOnlineContactsBusy = 0;
    gOnlinePanel = gUIRootWindow->Build(gOnlinePanelScript, OnlinePanelMsg);
    if (gOnlinePanel != nullptr) {
        InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
        sub->SetCallbackFunction(OnlinePanelSubPanelMsg);
        InterfaceListControl2* contacts = static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124));
        contacts->SetColumnFormatsAll(ContactsLCFormat, ContactsLCPostFormat);
        contacts->SetColumnType(0, static_cast<LCDataType>(0));
        contacts->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        contacts->SetCallbackFunction(ContactListHandler);
        InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(contacts->GetWindow(0xc3125));
        checks->SetCurrent(0xc3126);
        InterfaceListControl2* messages = static_cast<InterfaceListControl2*>(sub->GetWindow(0xc312b));
        messages->SetColumnFormatsAll(MessagesLCFormat, MessagesLCPostFormat);
        messages->SetColumnType(0, static_cast<LCDataType>(0));
        InterfaceWindow* edit = messages->GetWindow(0xc312c);
        edit->SetCallbackFunction(OnlineEditMsg);
        InterfaceCursorPosTextObjectSprite* sprite = new InterfaceCursorPosTextObjectSprite(nullptr);
        edit->SetClientDrawObject(sprite);
        edit->SetFont(GetFontTableEntry(gOnlineEditFontIndex));
        static_cast<InterfaceEditBox*>(edit)->SetStaticTextArea(gOnlineEditText, 0xff);
        static_cast<InterfaceEditBox*>(edit)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        edit->SetText(gOnlineEmptyHelp);   // the virtual at table offset 0x2c
        gOnlinePanelShown = 1;
        CoolRect rect;
        gOnlinePanel->GetPosition(&rect, 1);
        gGameWindowCentreX = gScreenCentreX;
        gGameWindowCentreY = gScreenCentreY;
        gGameWindowHalfWidth = gViewCentreX;
        gGameWindowHalfHeight = gViewCentreY;
    }
    gOnlineWindowE90 = gUIRootWindow->Build(gOnlineUnderlayScriptA, ButtonsUnderlayMsg);
    gOnlineWindowE90->Show(0);
    gOnlineWindowE90->unlink();
    gOnlineWindowE90->GetWindow(0x1789)->SetCallbackFunction(ButtonsMenuMsg);
    gOnlineWindowE8c = gUIRootWindow->Build(gOnlineUnderlayScriptB, ButtonsUnderlayMsg);
    gOnlineWindowE8c->Show(0);
    gOnlineWindowE8c->unlink();
    gOnlineWindowE8c->GetWindow(0x3003)->SetCallbackFunction(ButtonsMenuMsg);
    gOnlineWord88 = 0;
    gOnlineWindow = gUIRootWindow->CreateObject(1, 0, static_cast<int32_t>(0xfff04d2a), 0, 0, 0x7ff, 0x5ff);
    gOnlineWindow->SetCallbackFunction(OnlinePanelMsg);
    MoveHelpText(-1, 10);
    gOnlineCursorState = 0;
    gOnlineByte78 = 0;
}

// The park find panel's message function. 0x14: clears the word and gives the keyboard back; 0x1f: the edit box (0xe05a) gives it up;
// 0x100 with the search button (0xe059): 1 starts the search, else a debug text; with -2 closes; 0x400: the park of the list row opens its
// info panel; 0x802: presses the search button; 0x1000b with 0x1b: closes. Everything else is the default handler's.
int32_t OnlineParkFindPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    bool close = false;
    if (message == 0x100) {
        if (a == 0xe059) {
            if (b == 1) {
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();   // the original asks for 0x404 bytes
                gOnlineSystem->mParksByPlayer->GetList(gFindPanelPlayerName, FindParksPostGetPlayerParksCallbackFn, nullptr);
                ShowFindListSearching();
            } else {
                SamsUtilities::UDebug(kStopSearchText);
            }
        } else if (a < 0xe059 && a == -2) {
            close = true;
        }
    } else if (message < 0x100) {
        if (message != 0x15) {
            if (message < 0x15) {
                if (message >= 0x14) {
                    gFindPanelWord = 0;
                    gLobby->mWindow->CaptureKb(0);
                }
            } else if (message == 0x1f) {
                window->GetWindow(0xe05a)->CaptureKb(0);
            }
        }
    } else if (message == 0x802) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window->GetWindow(0xe059));
        if (static_cast<int16_t>(button->GetState()) == 0) {
            UI_PostMessage(window, 0x100, 0xe059, 1);
            static_cast<InterfaceButton*>(window->GetWindow(0xe059))->SetState(1, 1);
            static_cast<InterfaceEditBox*>(window->GetWindow(a))->SelectAll();
        }
    } else if (message < 0x802) {
        if (message == 0x400) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
            if (list != nullptr && b != -1) {
                int32_t data;
                list->GetData(b, &data);
                if (data != 0 && data != -1)
                    CreateOnlineParkInfoPanel(reinterpret_cast<const CFSOnlinePark*>(static_cast<uintptr_t>(static_cast<uint32_t>(data))), window,
                                              nullptr, GeneralParkDownloadCompleteCallback, nullptr);
                return 0;
            }
        }
    } else if (message == 0x1000b && a == 0x1b) {
        close = true;
    }
    if (close) {
        RemoveOnlineParkFindPanel();
        if (gFindPanelCloseCallback != nullptr) gFindPanelCloseCallback();
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
