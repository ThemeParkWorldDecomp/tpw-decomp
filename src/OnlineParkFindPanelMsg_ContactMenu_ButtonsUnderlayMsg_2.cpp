// Unit v51037, main app: the buttons menu underlay's message function and its hide function (Mac file ui_onlineParkFindPanel.cpp), the static
// initialisers of the online park info and list panels, RemoveOnlineParkListPanel, and four small UIInterface functions.
#include <cstdint>
#include <new>
#include <string>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_tooltip.h"

#include "Debug.h"
#include "MainWindowCallback.h"
#include "UIInterface.h"
#include "World.h"
#include "ui_publishParkPanel.h"

// Callees no header declares (untranslated stand-ins, or defined in a file with a local declaration).
void LocatorByeByeKid(CThing* thing);                                       // 0x101bf98c (56 bytes, src/gameplay_leaves.cpp)
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);      // 0x101a6234

// ---- the new globals (names invented): the objects the two static initialisers build and their __register_global_object records.
alignas(16) uint8_t gOnlineParkInfoStringA[64];   // 0x10396208
alignas(16) uint8_t gOnlineParkInfoNodeA[16];     // 0x1039621c
alignas(16) uint8_t gOnlineParkInfoStringB[64];   // 0x103961e8
alignas(16) uint8_t gOnlineParkInfoNodeB[16];     // 0x103961fc
alignas(16) uint8_t gOnlineParkInfoNodePark[16];  // 0x103961d8
alignas(16) uint8_t gOnlineParkInfoParkDtor[16];  // stands for the destructor word 0x10267550 (a TVector of CFSOnlinePark::~CFSOnlinePark)
alignas(16) uint8_t gOnlineParkListStringA[64];   // 0x10396258
alignas(16) uint8_t gOnlineParkListNodeA[16];     // 0x1039626c
alignas(16) uint8_t gOnlineParkListStringB[64];   // 0x10396238
alignas(16) uint8_t gOnlineParkListNodeB[16];     // 0x1039624c
static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= 64, "string storage");

namespace {
// The literals of the string pools at 0x1024f08a (+0x83, +0x91) and 0x1024f170 (+0x37, +0x45).
const char16_t kInfoTextA[] = u"upload";
const char16_t kInfoTextB[] = u"gms.dat";
const char16_t kListTextA[] = u"upload";
const char16_t kListTextB[] = u"gms.dat";

InterfaceWindow* WindowFromWord(int32_t word) {
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(word)));
}
}  // namespace

// 0x101e8b18 (88 bytes): hides the menu window the word gOnlineWord88 holds and gives the keyboard back (to the online panel's window).
void HideButtonsMenu() {
    WindowFromWord(gOnlineWord88)->Show(0);
    WindowFromWord(gOnlineWord88)->unlink();
    gOnlinePanel->CaptureKb(0);
}

// 0x101e86c4 (180 bytes)
int32_t ButtonsUnderlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        HideButtonsMenu();
    } else if (message < 0x10006 && message == 0x14) {
        if (window == gOnlineWindowE90) {
            gOnlineWindowE90 = nullptr;
        } else {
            gOnlineWindowE8c = nullptr;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101ebae0 (144 bytes)
void __sinit_ui_onlineParkInfoPanel_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* a = ::new (gOnlineParkInfoStringA) TbDynamicStringTemplate<char16_t>(kInfoTextA);
    __register_global_object(a, dtor, gOnlineParkInfoNodeA);
    void* b = ::new (gOnlineParkInfoStringB) TbDynamicStringTemplate<char16_t>(kInfoTextB);
    __register_global_object(b, dtor, gOnlineParkInfoNodeB);
    CFSOnlinePark* park = ::new (gOnlineParkInfoParkStorage) CFSOnlinePark();
    __register_global_object(park, gOnlineParkInfoParkDtor, gOnlineParkInfoNodePark);
}

// 0x101ec840 (156 bytes)
void RemoveOnlineParkListPanel() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes on the Mac
    gOnlineSystem->StopProcessing();
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gOnlineParkListPanel, 4, 0, 0);
}

// 0x101ec90c (116 bytes)
void __sinit_ui_onlineParkListPanel_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* a = ::new (gOnlineParkListStringA) TbDynamicStringTemplate<char16_t>(kListTextA);
    __register_global_object(a, dtor, gOnlineParkListNodeA);
    void* b = ::new (gOnlineParkListStringB) TbDynamicStringTemplate<char16_t>(kListTextB);
    __register_global_object(b, dtor, gOnlineParkListNodeB);
}

// 0x101efe60 (80 bytes): the day, the month plus one and the year of the date block (the other words are not read).
void UIInterface::DateChange(ParkDate date) {
    UI_MainPanelDateChange(date.mDay, date.mMonth + 1, date.mYear);
}

// 0x101f07b4 (76 bytes): the panel's word at +0x0c (its window) tells whether a staff member is shown.
void UIInterface::Screen_StaffLoc() {
    if (reinterpret_cast<UIStatusPanelFields*>(&gStaffStatusPanel)->mWindow.mOffset != 0) {
        ShowStaffPanel(gStaffRoomPanelThing);
    } else {
        ShowStaffPanel(nullptr);
    }
}

// 0x101f0834 (72 bytes)
void UIInterface::Screen_PeepLoc() {
    if (reinterpret_cast<UIStatusPanelFields*>(&gVisitorStatusPanel)->mWindow.mOffset != 0) {
        ShowStaffPanel(gStaffRoomPanelThing);
    } else {
        ShowKidLocatorPanel();
    }
}

// 0x101f0c2c (140 bytes): a kid that dies leaves the panels.
void UIInterface::ThingDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind == 1) {
        gVisitorStatusPanel.KidGone(t);
        AllKidsPanel_KidGone(t);
        LocatorByeByeKid(t);
    }
}
