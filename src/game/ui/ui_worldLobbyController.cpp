// ui_worldLobbyController.cpp: name proven by __sinit_ui_worldLobbyController_cpp in the main application.

#include "DrawRenderClasses.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MslWStringTree.h"
#include "online.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
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
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v5907: online.cpp's static initialiser (0x1022a06c), CheckShifts (0x1022a3b4) and the deque assign
// std::__cdeque<const void*>::do_assign<__cdeque<vector<Argument>*>::iterator> (0x10229ae4).

namespace NS_MacDoze { 
int16_t GetKeyState(int32_t key); 
}   // glue 0x10239280

// 0x1022a3b4 (116): the Shift (0x10), Control (0x11) and Alt (0x12) keys held, as two bits each.
int16_t CheckShifts() {
    uint32_t shifts = 0;
    if (NS_MacDoze::GetKeyState(0x10) < 0) shifts = 3;
    if (NS_MacDoze::GetKeyState(0x11) < 0) shifts |= 0xc;
    if (NS_MacDoze::GetKeyState(0x12) < 0) shifts |= 0x30;
    return static_cast<int16_t>(shifts);
}

// ui_worldLobbyController.cpp (main app): the lobby's chat satellite: the players of the park list are asked of the chat server
// (ProcessGetChatSatellitePlayers, answered through ChatServerCallbackFn) and the world lobby window's messages.

extern OnlineConfig gOnlineConfig;   // 0x102b2b70 (TPMain.h)

int32_t gChatSatelliteFinished;       // 0x10396fb4
int32_t gChatSatelliteAnswers;        // 0x10396fb8
int32_t gChatSatelliteValues[5];      // 0x10396fbc
int32_t gChatSatelliteParkIDs[5];     // 0x10396fd0
void* gChatSatelliteChat;             // 0x10396fe4
int32_t gChatSatelliteWanted;         // 0x10396fe8

namespace {

// The online system is made on first use, at every place that needs it (the Mac inlines this).
void EnsureOnlineSystem() {
    if (gOnlineSystem == nullptr) {
        void* block = online_callee::OperatorNew(0x404);
        if (block != nullptr) outbox_callee::OnlineSystemCtor(block);
        gOnlineSystem = static_cast<CFSOnlineSystem*>(block);
    }
}

const char kConnectionFailed[] = "ONLINE CHATELLITES: connection failed!\n";   // 0x102518b0

}  // namespace

// 0x1022a44c (440): the chat server's answers. 100 is a player line: command 7 gives a park id (argument 3) and the count
// (argument 4), command 0x2c (with a second argument) says the park's players left. -5 and -4 are the connection errors.
void ChatServerCallbackFn(int32_t id, void* unused, ArgumentVector& arguments) {
    (void)unused;
    if (id == 100) {
        Argument* data = arguments.mData;
        if (data[0].mCommand == 7) {
            MslCharString whole;
            chat_satellite_callee::ArgumentStrVal(&whole, &data[3]);
            MslCharString text;
            msl_char_string::construct(&text, &whole, 0, 0xffffffffu);
            online_callee::StringDtor(&whole, -1);
            int32_t value = static_cast<int32_t>(data[4].mCommand);
            int32_t park = atoi(text.mRep->data);
            for (int32_t i = 0; i < 5; i++) {
                if (park == gChatSatelliteParkIDs[i]) {
                    gChatSatelliteValues[i] = value;
                    break;
                }
            }
            online_callee::StringDtor(&text, -1);
        } else if (data[0].mCommand == 0x2c && arguments.mSize > 1) {
            MslCharString whole;
            chat_satellite_callee::ArgumentStrVal(&whole, &data[1]);
            MslCharString text;
            msl_char_string::construct(&text, &whole, 0, 0xffffffffu);
            online_callee::StringDtor(&whole, -1);
            int32_t park = atoi(text.mRep->data);
            for (int32_t i = 0; i < 5; i++) {
                if (park == gChatSatelliteParkIDs[i]) {
                    gChatSatelliteValues[i] = 0;
                    break;
                }
            }
            online_callee::StringDtor(&text, -1);
        }
        gChatSatelliteAnswers = gChatSatelliteAnswers + 1;
        if (gChatSatelliteAnswers >= 5) gChatSatelliteFinished = 1;
    } else if (id < 100 && id < -3 && id >= -5) {
        gChatSatelliteFinished = 1;
    }
}

// 0x1022a668 (776): when wanted, connects to the chat server and asks for the room of each of the (up to five) parks of the city
// list; once the answers are in (or the connection failed) releases the chat and sets the satellite panel's values.
void ProcessGetChatSatellitePlayers() {
    if (gChatSatelliteWanted != 0) {
        EnsureOnlineSystem();
        if (gOnlineSystem->mParksInCity2->mF28 != 0) {
            EnsureOnlineSystem();
            if (gOnlineSystem->mParksInCity2->mList->mCount > 0) {
                gChatSatelliteWanted = 0;
                EnsureOnlineSystem();
                int32_t count;
                if (gOnlineSystem->mParksInCity2->mList->mCount < 5) {
                    count = CFSOnlineSystem::Instance()->mParksInCity2->mList->mCount;
                } else {
                    count = 5;
                }
                for (int32_t i = 0; i < count; i++) {
                    EnsureOnlineSystem();
                    CFSOnlinePark* park = gOnlineSystem->mParksInCity2->ReturnParkInfo(i);
                    gChatSatelliteParkIDs[i] = park->mParkID;
                    gChatSatelliteValues[i] = 0;
                }
                void* chat = online_callee::OperatorNew(0xc);
                if (chat != nullptr) online_callee::ChatCtor(chat);
                gChatSatelliteChat = chat;
                if (chat == nullptr) {
                    chat = online_callee::OperatorNew(0xc);
                    if (chat != nullptr) online_callee::ChatCtor(chat);
                    gChatSatelliteChat = chat;
                }
                int32_t connected = chat_satellite_callee::ChatConnect(
                    gChatSatelliteChat, gOnlineConfig.mChatServer.c_str(), static_cast<uint16_t>(gOnlineConfig.mChatPort),
                    ChatServerCallbackFn, nullptr);
                if (connected == 0) {
                    SamsUtilities::UDebug(kConnectionFailed);
                } else {
                    gChatSatelliteFinished = 0;
                    gChatSatelliteAnswers = 0;
                    for (int32_t i = 0; i < count; i++) {
                        TbDynamicStringTemplate<char> text;
                        text.Format("%d", gChatSatelliteParkIDs[i]);
                        MslCharString room;
                        outbox_callee::StringCtor(&room, text.mData);
                        chat_satellite_callee::ChatRoomInfo(gChatSatelliteChat, room, nullptr);
                        online_callee::StringDtor(&room, -1);
                    }
                }
            }
        }
    }
    if (gChatSatelliteFinished != 0) {
        online_callee::ChatRelease(gChatSatelliteChat);
        for (int32_t i = 0; i < 5; i++) UpdateSatellite(i, gChatSatelliteValues[i]);
        gChatSatelliteFinished = 0;
    }
}

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// ---- storage of the static initialisers (the Mac addresses are in the comments; the names are invented)
alignas(16) uint8_t gWorldLobbyControllerText[0x30];   // 0x10396ff0
uint8_t gWorldLobbyControllerTextNode[12];             // 0x10397004
// 0x1022a9a8: the download globe's virtual slot 0x4c
void FindParkOnlineParkInfoPanelCancelCallback() { parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe); }

// 0x1022aa20 (972): the world lobby window's messages: the arrow keys (0x16001 to 0x16005) turn the globe (the download globe,
// or the publish one when the window's extra parameter 0 is set) and 0x16006 zooms it; 0x16000 (message 0x100) goes back to the
// theme islands; 0x16003 opens the park find panel once the lists are in; message 0x14 forgets the satellite panel's parent.
int32_t WorldLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    bool publish = window->getExtraParam(0) != nullptr;
    auto globe = [publish]() -> CLBScreenComponentOnlineGlobe* {
        return publish ? static_cast<CLBScreenComponentOnlineGlobe*>(gLobbyScreens->mPublishGlobe)
                       : static_cast<CLBScreenComponentOnlineGlobe*>(gLobbyScreens->mDownloadGlobe);
    };
    if (message == 0x100) {
        if (a == 0x16003) {
            EnsureOnlineSystem();
            if (gOnlineSystem->AreWeProcessing() == 0) {
                EnsureOnlineSystem();
                if (gOnlineSystem->mCityList->mF28 != 0) {
                    EnsureOnlineSystem();
                    if (gOnlineSystem->mThemeList->mF28 != 0) {
                        EnsureOnlineSystem();
                        if (gOnlineSystem->mServerList->mF28 != 0) {
                            gLobbyScreens->mDownloadGlobe->Slot50();
                            CreateOnlineParkFindPanel(FindParkOnlineParkInfoPanelCancelCallback);
                        }
                    }
                }
            }
        } else if (a == 0x16000) {
            gLobbyScreens->mDownloadGlobe->Hide();
            gLobbyScreens->mThemeIslands->Show();
            AdvisorResponse response;
            response.mId = 600;
            gLobbyScreens->mAdvisor->Add(response, 1);
            return 0;
        } else if (a == 0x16006) {
            if (b == 1) {
                globe()->GlobeZoomIn();
            } else {
                globe()->GlobeZoomOut();
            }
        }
    } else if (message < 0x100) {
        if (message == 0x14) gUISatelliteParent = nullptr;
    } else if (message < 0x1000a && message >= 0x10008) {
        switch (a) {
            case 0x16001: globe()->GlobeDown(); break;
            case 0x16002: globe()->GlobeLeft(); break;
            case 0x16004: globe()->GlobeRight(); break;
            case 0x16005: globe()->GlobeUp(); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v5948: the world lobby window's creation and the satellite number windows' message function (main app).
//   CreateWorldLobbyController 0x1022b038 (1600), SatelliteMsg 0x1022ae30 (464)

int16_t gWorldLobbyScript[2] = {1, 0};   // 0x102b20f8

// Weak; the test library fakes it (the constructor is another file's).
namespace worldlobby_callee {
__attribute__((weak)) InterfaceDrawObject* NewNumberText() {
    void* memory = ::operator new(std::max<size_t>(0x19c, sizeof(InterfaceNumberTextObject)));   // the host object is larger than the Mac's
    lender_callee::NumberCtor(memory);   // 0x1018d930 with NULL
    return static_cast<InterfaceDrawObject*>(memory);
}
}  // namespace worldlobby_callee

namespace {

// The lazy creation the Mac repeats before every use of the online system.
CFSOnlineSystem* Sys() {
    if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
    return gOnlineSystem;
}

// One of the satellite panel's number windows: a number text object, the small font, value 0 twice, SatelliteMsg, its number, shown.
void MakeNumberWindow(InterfaceWindow* window, int32_t number) {
    window->SetClientDrawObject(worldlobby_callee::NewNumberText());
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
    window->SetFontJust(1, 1);
    lender_callee::WindowSetValue(window, 0);
    window->SetCallbackFunction(SatelliteMsg);
    window->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(number)));
    entry_callee::WindowEnable(window, 1);
}

}  // namespace

// 0x1022ae30
int32_t SatelliteMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (Sys()->mCityList->mF28 != 0) {
            if (Sys()->mThemeList->mF28 != 0) {
                if (Sys()->mServerList->mF28 != 0) {
                    if (Sys()->mParksInCity2->mF28 != 0) {
                        int32_t number = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
                        Sys()->EnterChatSatellite(number - 1);
                    }
                }
            }
        }
    } else if (message == 0x1e) {
        ProcessGetChatSatellitePlayers();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
// 0x1022b038
void CreateWorldLobbyController(bool publish) {
    if (gUISatelliteParent != nullptr) return;
    if (gLobby->mWindow == nullptr) return;
    gUISatelliteParent = gLobby->mWindow->Build(gWorldLobbyScript, WorldLobbyControllerMsg);
    gUISatelliteParent->setExtraParam(0, reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint8_t>(publish))));
    if (publish) {
        gUISatelliteParent->GetWindow(0x16007)->Show(0);
        entry_callee::WindowEnable(gUISatelliteParent->GetWindow(0x16003), 0);
        entry_callee::WindowEnable(gUISatelliteParent->GetWindow(0x16000), 0);
    } else {
        InterfaceWindow* panel = gUISatelliteParent->GetWindow(0x16007);
        entry_callee::WindowEnable(panel, 1);
        for (int32_t i = 0; i < 5; ++i) MakeNumberWindow(panel->GetWindow(0x16008 + i), i + 1);
        gChatSatelliteWanted = 1;
    }
    static const int32_t kButtons[4] = {0x16002, 0x16004, 0x16005, 0x16001};
    for (int32_t id : kButtons) static_cast<InterfaceButton*>(gUISatelliteParent->GetWindow(id))->SetAutoRepeat(0, 10);
}

// 0x1022b6ac
void RemoveWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 4, 0, 0); }

// ---- the world lobby controller: the satellite parent window is the controller itself
// 0x1022b718
void ShowWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 6, 1, 0); }
// 0x1022b780
void HideWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 6, 0, 0); }

// Unit v51044: small functions of the online, lobby and 2D map code:
//   0x1021eb7c std::less<std::wstring>::operator()          0x1021ecb8 std::allocator<nocase_string>::destroy
//   0x1021e4dc std::basic_streambuf<wchar_t>::uflow         0x10222c78 std::allocator<pair<const nocase_string, BuddyInfo>>::destroy
//   0x102238d4 std::allocator<std::wstring>::destroy        0x10227840 std::pair<const nocase_string, BuddyInfo>::pair (copy)
//   0x1022b7e8 ShowWorldLobbySats  0x1022b85c HideWorldLobbySats  0x1022ba30 WorldLobbySetZoomButton
//   0x10230d64 CUI2DMapSprite::CUI2DMapSprite

// 0x1022b7e8
void ShowWorldLobbySats() {
    UI_SendMessage(gUISatelliteParent->GetWindow(0x16007), 6, 1, 0);
}

// 0x1022b85c
void HideWorldLobbySats() {
    UI_SendMessage(gUISatelliteParent->GetWindow(0x16007), 6, 0, 0);
}

// Unit v5193: UpdateSatellite 0x1022b8d0 and UI2DMReturnScaleColour 0x1022bb44 (ui_worldLobbyController.cpp, ui_2dmap.cpp).

InterfaceWindow* gUISatelliteParent;   // 0x10396fec

void UpdateSatellite(int32_t which, int32_t value) {
    if (gUISatelliteParent == nullptr) return;
    InterfaceWindow* panel = gUISatelliteParent->GetWindow(0x16007);
    int32_t id;
    switch (which) {
        case 0: id = 0x16008; break;
        case 1: id = 0x16009; break;
        case 2: id = 0x1600a; break;
        case 3: id = 0x1600b; break;
        case 4: id = 0x1600c; break;
        default: return;
    }
    // The Mac calls slot 7 of the vtable at +0x118 with the window itself as this.
    panel->GetWindow(id)->SetValue(value);
}

// 0x1022ba30
void WorldLobbySetZoomButton(int32_t zoom) {
    InterfaceWindow* parent = gUISatelliteParent;
    if (parent == nullptr) return;
    int8_t state = (zoom == 1) ? 1 : 0;
    static_cast<InterfaceButton*>(parent->GetWindow(0x16006))->SetState(state, 1);
}

// 0x1022bacc
void __sinit_ui_worldLobbyController_cpp() {
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gWorldLobbyControllerText, "storage");
    auto* text = ::new (gWorldLobbyControllerText) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gWorldLobbyControllerTextNode);
}
