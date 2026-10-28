// ui_cityLobbyController.cpp: name proven by __sinit_ui_cityLobbyController_cpp in the main application.

#include "AdvisorScoring.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/msl.h"
#include "std_basic_filebuf.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "WindowScripts.inc"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

// 0x101e0388, 56 bytes: the virtual at slot 0x30 of the online islands screen (the existing empty body elsewhere is wrong).
void ListParkOnlineParkInfoPanelCancelCallback() { parkinfo_callee::OnlineIslandsSlot30(gLobbyScreens->mOnlineIslands); }

// ui_cityLobbyController.cpp (main app): the city lobby's controller window. UpdateCityLobbyUI 0x101e0928 (552 bytes), CreateCityLobbyController
// 0x101e05e4 (476 bytes) and CityLobbyControllerMsg 0x101e0400 (416 bytes). The neighbour file has the file's static initialiser.
//
// The window is built under the lobby's window (gLobby's word at +4) from the description at 0x102afe7a; its two text windows (ids 0x49d9 and
// 0x49da) get title text objects. UpdateCityLobbyUI shows or hides them for the island and writes the island number and the park's name
// (through the swear filter when the master save has it on). The message function sends the buttons 0x49d5 to 0x49dc to the online islands
// screen of the lobby and forgets the window when it dies.

InterfaceWindow* gCityLobbyWindow;                  // 0x10395c70 (name invented)
int16_t gCityLobbyScript[] = {TPW_WINDOW_SCRIPT_102AFE7A};   // 0x102afe7a (name invented): the words are the image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc)

// ListParkOnlineParkInfoPanelCancelCallback (0x101e0388) is translated in src/PublishPanelMsg_OnlineParkListPostGetAllParksInCityCallbackFn.cpp.

namespace {

// The master save, made on first use (new 0x4c bytes, then the constructor; the Mac's new never answers NULL).
CFSGlobalMasterSave* MasterSaveOnFirstUse() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    return gMasterSave;
}

// The dressing of one of the controller's two text windows (the first also has its text emptied and the title colour given first).
InterfaceTitleTextObject* NewTitleObject() { return new InterfaceTitleTextObject(nullptr); }

// The string's characters: the representation's word at +0xc is a 32-bit address (on the Mac the whole address). On the host the arena
// block may sit above 4 GB, so the upper half is taken from the representation's own address (same block of memory).
char16_t* StringData(MslStrRep* rep) {
    uintptr_t high = reinterpret_cast<uintptr_t>(rep) & ~static_cast<uintptr_t>(0xffffffffu);
    return reinterpret_cast<char16_t*>(high | rep->data);
}

}  // namespace

// 0x101e0400: message 0x14 forgets the window; message 0x100 with a button id from 0x49d5 to 0x49dc runs the jump table at 0x102b004c (the
// handlers below, by the order of their code); everything else, and the handlers that fall through, go to the default handler.
int32_t CityLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        uint32_t index = static_cast<uint32_t>(a - 0x49d5);
        if (index <= 7) {
            CLBScreenComponentOnlineIslands* online = gLobbyScreens->mOnlineIslands;
            switch (index) {   // the table's targets: 0x101e04f8 for 0, see the cases
                case 0:        // 0x101e04f8
                    OnlineIslandsSlot34(online);
                    CreateOnlineParkListPanel(&ListParkOnlineParkInfoPanelCancelCallback);
                    return 0;
                case 1:        // 0x101e0524
                    gLobbyScreens->mOnlineIslands->Hide();
                    gLobbyScreens->mThemeIslands->Show();
                    gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x258}, 1);
                    return 0;
                case 2:        // 0x101e04a4
                    gLobbyScreens->mOnlineIslands->Hide();
                    gLobbyScreens->mDownloadGlobe->Show();
                    return 0;
                case 3:        // 0x101e04dc
                    OnlineIslandsSlot48(online);
                    break;
                case 6:        // 0x101e046c
                    online->GotoPreviousIsland();   // slot 0x44
                    break;
                case 7:        // 0x101e0488
                    online->GotoNextIsland();       // slot 0x40
                    break;
                default:       // 4 and 5 go to 0x101e0574
                    break;
            }
        }
    } else if (message < 0x100 && message == 0x14) {
        gCityLobbyWindow = nullptr;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
// 0x101e05e4: builds the window once, when the lobby has a window to build it under, and dresses its two text windows.
void CreateCityLobbyController() {
    if (gCityLobbyWindow != nullptr) return;
    InterfaceWindow* parent = gLobby->mWindow;
    if (parent == nullptr) return;
    gCityLobbyWindow = parent->Build(gCityLobbyScript,&CityLobbyControllerMsg);

    InterfaceWindow* title = gCityLobbyWindow->GetWindow(0x49d9);
    title->SetClientDrawObject(NewTitleObject());
    title->SetFont(GetFontTableEntry(5));
    title->SetFontJust(1, 1);
    title->SetText(nullptr);
    UI_TITLE_COLOUR(title);
    title->SetFont(GetFontTableEntry(0));
    title->SetFontColour(0xfc, 0xe8, 7, 0xff);

    InterfaceWindow* number = gCityLobbyWindow->GetWindow(0x49da);
    number->SetClientDrawObject(NewTitleObject());
    number->SetFont(GetFontTableEntry(0));
    number->SetFontJust(1, 1);
    number->SetFontColour(0xfc, 0xe8, 7, 0xff);
}

// 0x101e07f0, 56 bytes.
void RemoveCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 4, 0, 0); }

// 0x101e0858, 56 bytes.
void ShowCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 6, 1, 0); }

// 0x101e08c0, 56 bytes.
void HideCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 6, 0, 0); }

// 0x101e0928: `island` NULL (or one without a park) hides both text windows, otherwise they show the island's number (the string at 0x1024edc2
// with order + 1) and the park's name (filtered when the master save's swear filter is on). Nothing but the local string's making and
// breaking happens when there is no window.
void UpdateCityLobbyUI(CLBIsland* island) {
    MslStrRep* text = nullptr;   // a local std::wstring: one pointer to the representation (the data pointer at +0xc)
    online_callee::WStringCtor(&text);
    if (gCityLobbyWindow == nullptr) {
        online_callee::WStringDtor(&text, -1);
        return;
    }
    if (island == nullptr || island->mPark == nullptr) {
        gCityLobbyWindow->GetWindow(0x49da)->Show(0);
        gCityLobbyWindow->GetWindow(0x49d9)->Show(0);
    } else {
        gCityLobbyWindow->GetWindow(0x49da)->Show(1);
        gCityLobbyWindow->GetWindow(0x49d9)->Show(1);
        swprintf(gUIMainConvertBuffer, 0x3fff, reinterpret_cast<const char16_t*>(gCityLobbyStringPool), island->mOrder + 1);
        gCityLobbyWindow->GetWindow(0x49da)->SetText(gUIMainConvertBuffer);
        const char16_t* name = island->mPark->mText0C.mData;   // the Mac's word at the park's +0x14
        online_callee::WStringAssign(&text, name, wcslen(name));
        if (MasterSaveOnFirstUse()->IsSwearFilterOn()) {
            // The Mac filters the string in place (this = the master save, whose first two members are the filter's lists).
            char16_t* data = StringData(text);
            std::u16string copy(data, static_cast<size_t>(text->len));
            reinterpret_cast<SwearFilter*>(MasterSaveOnFirstUse())->Filter(copy, true);
            std::memcpy(data, copy.data(), copy.size() * sizeof(char16_t));
        }
        char16_t* shown = StringData(text);
        gCityLobbyWindow->GetWindow(0x49d9)->SetText(shown);
    }
    online_callee::WStringDtor(&text, -1);
}

// 0x101e0cec (116 bytes): static initialiser of ui_cityLobbyController.cpp. Builds two global dynamic wide strings from
// literals in the file's string pool (pool + 0x3d and pool + 0x4b) and registers each with its destructor for exit.

// The three externs (string pool, destructor, constructor 0x10238800) are declared in AdvisorScoring.h.

// The launcher's definitions of the two glue names; the unit test library overrides them with recording fakes. The real string
// members are reached through their symbols (weak, so the test library links without the string sources).
extern "C" __attribute__((weak)) void RealCityLobbyStringCtor(void* self, const char16_t* text) __asm__("__ZN23TbDynamicStringTemplateIDsEC1EPKDs");
extern "C" __attribute__((weak)) void RealCityLobbyStringDtor(void* self) __asm__("__ZN23TbDynamicStringTemplateIDsED1Ev");
extern "C" __attribute__((weak)) void* CityLobbyStringConstruct(void* self, const char16_t* text) {
    RealCityLobbyStringCtor(self, text);
    return self;
}
extern "C" __attribute__((weak)) void CityLobbyStringDestructor(void* object, int16_t deleteFlag) {
    (void)deleteFlag;
    RealCityLobbyStringDtor(object);
}

alignas(8) static unsigned char gCityLobbyStringA[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395c94
alignas(8) static unsigned char gCityLobbyNodeA[16];                                            // 0x10395ca8
alignas(8) static unsigned char gCityLobbyStringB[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395c74
alignas(8) static unsigned char gCityLobbyNodeB[16];                                            // 0x10395c88

void* CityLobbyStringStorage(int which) { return which == 0 ? (void*)gCityLobbyStringA : (void*)gCityLobbyStringB; }
void* CityLobbyNodeStorage(int which) { return which == 0 ? (void*)gCityLobbyNodeA : (void*)gCityLobbyNodeB; }

void __sinit_ui_cityLobbyController_cpp() {
    void* a = CityLobbyStringConstruct(gCityLobbyStringA, reinterpret_cast<const char16_t*>(gCityLobbyStringPool + 0x3d));
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gCityLobbyNodeA);
    void* b = CityLobbyStringConstruct(gCityLobbyStringB, reinterpret_cast<const char16_t*>(gCityLobbyStringPool + 0x4b));
    __register_global_object(b, reinterpret_cast<void*>(&CityLobbyStringDestructor), gCityLobbyNodeB);
}

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gProgBarScript[] = {TPW_WINDOW_SCRIPT_102AFE24};           // 0x102afe24, CreateUIProgressBar 0x101dffd8
