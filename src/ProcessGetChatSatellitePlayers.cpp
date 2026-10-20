// ui_worldLobbyController.cpp (main app): the lobby's chat satellite: the players of the park list are asked of the chat server
// (ProcessGetChatSatellitePlayers, answered through ChatServerCallbackFn) and the world lobby window's messages.
#include <cstdint>
#include <cstdlib>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "email.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"

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
