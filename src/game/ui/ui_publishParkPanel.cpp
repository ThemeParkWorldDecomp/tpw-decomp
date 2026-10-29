// ui_publishParkPanel.cpp: name proven by __sinit_ui_publishParkPanel_cpp in the main application.

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "MslWStringTree.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTList.h"
#include "TbVTag.h"
#include "Text.h"
#include "TPGlobals.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

alignas(16) uint8_t gPublishParkPanelText[0x30];       // 0x1039639c
uint8_t gPublishParkPanelTextNode[12];                 // 0x103963b0
// 0x101ec9b8: the publish globe's virtual slot 0x34 (the arguments are not used)
void PostRequestUploadCallbackFunction(int32_t error, const void* context) {
    (void)error;
    (void)context;
    mainmenu_callee::PublishGlobeSlot34(gLobbyScreens->mPublishGlobe);
}

// Unit v5064: the publish park panel's message function and its creation, and the online park list panel's creation
// (0x101eca2c, 0x101eccc8, 0x101ec4e0).

// ---- the globals of the panels (no other file of the program that is translated defines them; names invented)
OnlineParkSelection* gOnlineParkSelection = nullptr;   // 0x102e4864
OnlineUploadFile gOnlineUploadFile = {};               // 0x102e4868
InterfaceWindow* gPublishParkPanel = nullptr;          // 0x10396278
char16_t gPublishParkDescription[0x80] = {};           // 0x1039627c
char16_t gPublishParkName[0x10] = {};                  // 0x1039637c
int16_t gPublishParkPanelScript[] = {TPW_WINDOW_SCRIPT_102B11E6};   // 0x102b11e6: the words are the image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc)
// 0x101eca2c, 608 bytes. The message function of the publish panel: 0x15 and 0x14 (the panel's show and hide, with the key tables and the
// lobby window's message 6), 0x100 (a button: -1 uploads the park, -2 closes), the edit boxes' 0x802 to 0x804 (a key in one box moves the
// keyboard to the other), everything else (and all of them but the two buttons) to the dialog's default handler. The game type object is
// built on first use here (the guard byte at 0x102b3aa4).
int32_t PublishPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == -1) {
            if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
            CFSOnlineSystem* system = gOnlineSystem;
            CGameType& type = TheGameType();
            system->RequestUpload(gOnlineUploadFile.mPath, gOnlineParkSelection->mID, gPublishParkName, gPublishParkDescription,
                                  type.mType == kGameTypeEasy, PostRequestUploadCallbackFunction, nullptr);
            RemovePublishParkPanel();
            return 0;
        }
        if (a == -2) {
            RemovePublishParkPanel();
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            UI_SendMessage(gLobby->mWindow, 6, 0, 0);
            UIMain_DisableAllKeyTables();
        } else if (message == 0x14) {
            gPublishParkPanel = nullptr;
            UIMain_EnableAllKeyTables();
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x803) {
        if (a == 0x11e4)
            window->GetWindow(0x11e3)->CaptureKb(0);
        else
            window->GetWindow(0x11e4)->CaptureKb(0);
    } else if (message < 0x803) {
        if (message >= 0x802 && a == 0x11e4) window->GetWindow(0x11e3)->CaptureKb(0);
    } else if (message < 0x805) {
        UI_PostMessage(window, 4, 0, 0);
    }
    return DefaultDialogCallback(window, message, a, b);
}

// 0x101eccc8, 628 bytes. Makes the publish panel: the dialog, one level above its parent, the name box (a cursor text object, the font 7,
// the park name's buffer of 0xf characters, the hilight cursor) with the localised theme name as text, the description box (the same with
// wrap, left justification and 0x7e characters) with the interface string 0xdc, the title (string 0xda) and the advisor's line 0x250.
// The text goes in through the window's virtual SetText (slot 0x2c of the vtable at 0x118).
void CreatePublishParkPanel() {
    static char kHilight[] = "hilight";
    UIMain_CheckPanel();
    gPublishParkPanel = Dialog(gPublishParkPanelScript, PublishPanelMsg, 0);
    gPublishParkPanel->setZ(static_cast<int16_t>(gPublishParkPanel->mZ + 4));
    InterfaceWindow* box = gPublishParkPanel->GetWindow(0x11e4);
    InterfaceCursorPosTextObjectSprite* object = new InterfaceCursorPosTextObjectSprite(nullptr);
    box->SetClientDrawObject(object);
    box->SetFont(GetFontTableEntry(7));
    static_cast<InterfaceEditBox*>(box)->SetStaticTextArea(gPublishParkName, 0xf);
    static_cast<InterfaceEditBox*>(box)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    TbDynamicStringTemplate<char16_t> themeName;
    gGame.GetLocalisedThemeName(gGame.GetCurrentThemeName(), themeName);
    box->SetText(themeName.mData);
    box = gPublishParkPanel->GetWindow(0x11e3);
    object = new InterfaceCursorPosTextObjectSprite(nullptr);
    box->SetClientDrawObject(object);
    box->SetFont(GetFontTableEntry(7));
    box->SetFontWrap(1, 0);
    box->SetFontJust(0, 0);
    static_cast<InterfaceEditBox*>(box)->SetStaticTextArea(gPublishParkDescription, 0x7e);
    static_cast<InterfaceEditBox*>(box)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    box->SetText(const_cast<char16_t*>(UIMain_GetString(0xdc)));
    UIMain_SetTitleText(gPublishParkPanel, 0x11e5, const_cast<char16_t*>(UIMain_GetString(0xda)));
    gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x250}, 1);
}

// 0x101ecf6c
void RemovePublishParkPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gPublishParkPanel, 4, 0, 0);
}
// ---- the publish park panel
// 0x101ecfe8
void __sinit_ui_publishParkPanel_cpp() {
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gPublishParkPanelText, "storage");
    auto* text = ::new (gPublishParkPanelText) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gPublishParkPanelTextNode);
}
