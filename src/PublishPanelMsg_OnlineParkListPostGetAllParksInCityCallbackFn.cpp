// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.
#include "ui_publishParkPanel.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

#include "AdvisorScoring.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "email.h"
#include "online.h"
#include "ui_Postcard.h"

// Globals of this unit's files without a known name (invented); the Mac addresses in the comments.
extern InterfaceWindow* gOnlineNewsPanel;   // 0x10395dd8 (defined in UI_IslandLobby_TextPanelMsg.cpp)
extern const char16_t gOnlineNewsText[];
alignas(16) const char16_t gOnlineNewsText[] = u"upload";   // 0x1024ee86: the literal given to the news string
uint8_t gOnlineNewsStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395ddc
uint8_t gOnlineNewsNodeA[16];                                                                         // 0x10395df0
uint8_t gOnlineLoginStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395db4
uint8_t gOnlineLoginNodeA[16];                                                                        // 0x10395dc8
uint8_t gOnlineCityInfoStringA[sizeof(TbDynamicStringTemplate<char16_t>)] __attribute__((aligned(8)));   // 0x10395d08
uint8_t gOnlineCityInfoNodeA[16];                                                                         // 0x10395d1c

// 0x101ec444, 56 bytes.
void OnlineParkListPostGetAllParksInCityCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks) {
    if (error == 0)
        FillParkList(parks);
    else
        RemoveOnlineParkListPanel();
}

// 0x101e9324, 48 bytes.
void FindParksPostGetPlayerParksCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks) {
    if (error == 0) FillOnlineParkFindList(parks);
}

// 0x101eba64, 76 bytes.
void RemoveOnlineParkInfoPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gOnlineParkInfoPanel, 4, 0, 0);
}

// 0x101e92c4, 56 bytes.
void RemoveOnlinePanel() { UI_SendMessage(gOnlinePanel, 4, 0, 0); }

// 0x101e82dc, 152 bytes: empties the contacts list, makes the online control on first use and has it refill the user list.
void FillContactList() {
    InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
    static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124))->RemoveAllItems();
    if (gOnlineControl == nullptr) gOnlineControl = contacts_callee::NewOnlineControl();
    gOnlineControl->UpdateUserList();
}

// 0x101e7c18, 180 bytes.
InterfaceWindow* MessagesLCFormat() {
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t height = static_cast<int16_t>(font->GetHeight());
    int16_t bottom = static_cast<int16_t>(height * 0x600 / gUIRealScreenHeight + 4);
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, bottom);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    window->SetCallbackFunction(MessagesLCEntryHandler);
    return window;
}

// 0x101e7cf4, 168 bytes.
void MessagesLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceTextObjectSprite*>(memory));
    window->SetFont(GetFontTableEntry(gContactsFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
}

// 0x101e4124, 20 bytes.
void OutboxCallback(EmailCompletionCode code, EmailErrorCode error) {
    gMailStatusA = code;
    gMailStatusB = error;
}

// 0x101e3628, 60 bytes.
void __sinit_ui_onlineNewsPanel_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineNewsStringA, gOnlineNewsText);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineNewsNodeA);
}

// 0x101e35c4, 56 bytes.
void RemoveOnlineNewsPanel() { UI_SendMessage(gOnlineNewsPanel, 4, 0, 0); }

// 0x101e2c78, 64 bytes.
void __sinit_ui_onlineLogin_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineLoginStringA, gOnlineLoginEmptyText + 1);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineLoginNodeA);
}

// 0x101e2450, 64 bytes.
void __sinit_ui_onlineCityInfoOverlay_cpp() {
    void* a = CityLobbyStringConstruct(gOnlineCityInfoStringA, kOnlineCityNoText + 1);
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gOnlineCityInfoNodeA);
}

// 0x101e23e8, 56 bytes.
void HideOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 6, 0, 0); }

// 0x101e2380, 56 bytes.
void ShowOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 6, 1, 0); }

// 0x101e2314, 56 bytes.
void RemoveOnlineCityInfoOverlay() { UI_SendMessage(gOnlineCityOverlay, 4, 0, 0); }

// 0x101e1ee8, 60 bytes: message 0x14 forgets the overlay.
int32_t CityInfoOverlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gOnlineCityOverlay = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e08c0, 56 bytes.
void HideCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 6, 0, 0); }

// 0x101e0858, 56 bytes.
void ShowCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 6, 1, 0); }

// 0x101e07f0, 56 bytes.
void RemoveCityLobbyController() { UI_SendMessage(gCityLobbyWindow, 4, 0, 0); }

// 0x101e0388, 56 bytes: the virtual at slot 0x30 of the online islands screen (the existing empty body elsewhere is wrong).
void ListParkOnlineParkInfoPanelCancelCallback() { parkinfo_callee::OnlineIslandsSlot30(gLobbyScreens->mOnlineIslands); }
