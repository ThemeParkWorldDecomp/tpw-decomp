// Unit v5550: the online park info panel's messages and the park find panel (ui_onlineParkInfoPanel.cpp, ui_onlineParkFindPanel.cpp):
//   OnlineParkInfoPanelMsg 0x101ea7f0, GeneralParkDownloadCompleteCallback 0x101ea734, VisitParkPostDownloadCallbackFn 0x101ea684,
//   __sinit_ui_onlineParkFindPanel_cpp 0x101ea5d8, UpdateParkFindPanelProgress 0x101ea4e4, RemoveOnlineParkFindPanel 0x101ea468
#include <cstdint>
#include <new>

#include "AdvisorScoring.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

InterfaceWindow* gOnlineParkFindPanel = nullptr;                    // 0x103960c8
InterfaceWindow* gOnlineParkInfoPanel = nullptr;                    // 0x103961e4
void (*gOnlineParkInfoClose)() = nullptr;                           // 0x10396114
const void* gVisitParkContext = nullptr;                            // 0x1039610c
void (*gVisitParkPostDownload)(int32_t, const void*) = nullptr;     // 0x10396110
alignas(16) uint8_t gOnlineParkInfoParkStorage[sizeof(CFSOnlinePark)];   // 0x10396118
alignas(16) uint8_t gOnlineParkFindStringA[0x40];                   // 0x103960cc
alignas(16) uint8_t gOnlineParkFindNodeA[0x10];                     // 0x103960e0
alignas(16) uint8_t gOnlineParkFindStringB[0x40];                   // 0x103960ec
alignas(16) uint8_t gOnlineParkFindNodeB[0x10];                     // 0x10396100

// The program's definitions of the two virtual calls (slots 0x30 and 0x4c of lobby screens, untranslated: nothing happens). Weak, so the
// unit's test library can define recording fakes.
namespace parkinfo_callee {
__attribute__((weak)) void OnlineIslandsSlot30(void*) {}
__attribute__((weak)) void DownloadGlobeSlot4c(void*) {}
}  // namespace parkinfo_callee

namespace {

// The wide strings of the pool at 0x1024f01e (+0x0a, +0x4d, +0x5b).
const char16_t kProgressFormat[] = u"%d %%";
const char16_t kTextB[] = u"upload";
const char16_t kTextA[] = u"gms.dat";

// 0x101ea6xx and the like: the panel is removed by the parent, or the parent is told to close (message 4).
void CloseFromParent(InterfaceWindow* window) {
    if (window->GetParent() == gLobby->mWindow)
        RemoveOnlineParkInfoPanel();
    else
        UI_SendMessage(window->GetParent(), 4, 0, 0);
}

CFSOnlineSystem* TheOnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    return gOnlineSystem;
}

// Escape (message 0x100 with -2, message 0x1000b with 0x1b): when the parent is the root window the lobby window is shown again; then
// the panel is removed and the caller's close function runs.
int32_t Escape(InterfaceWindow* window) {
    if (window->GetParent() == gUIRootWindow) UI_SendMessage(gLobby->mWindow, 6, 1, 0);
    RemoveOnlineParkInfoPanel();
    if (gOnlineParkInfoClose != nullptr) gOnlineParkInfoClose();
    return 0;
}

}  // namespace

// 0x101ea7f0, 896 bytes. 0x14 forgets the panel and gives the keyboard back; 0x100 with 0xe592e (visit) removes the panel and asks for
// the park's download, with 0xe592f (unpublish) unpublishes the park, removes the panel and shows the lobby's screen, with -2 (cancel)
// and 0x1000b with 0x1b (escape) close it. Everything else, and the cases that fall through, goes to the default window handler.
int32_t OnlineParkInfoPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CFSOnlinePark* park = reinterpret_cast<CFSOnlinePark*>(gOnlineParkInfoParkStorage);
    if (message == 0x100) {
        if (a == 0xe592e) {
            CloseFromParent(window);
            TheOnlineSystem()->RequestFTPDownload(*park, VisitParkPostDownloadCallbackFn, nullptr);
            return 0;
        }
        if (a < 0xe592e) {
            if (a == -2) return Escape(window);
        } else if (a < 0xe5930) {
            TheOnlineSystem()->UnpublishPark(park->mParkID);
            CloseFromParent(window);
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            if (mainmenu_callee::ScreenIsVisible(gLobbyScreens->mOnlineIslands))
                parkinfo_callee::OnlineIslandsSlot30(gLobbyScreens->mOnlineIslands);
            else
                parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe);
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x14) {
            gOnlineParkInfoPanel = nullptr;
            // The parent's destroying flag (the Mac's byte 0) by name: the host window has its vtable pointer first.
            if (window->GetParent() != nullptr && window->GetParent()->mDestroying == 0)
                window->GetParent()->CaptureKb(0);
            else
                UIMain_DecideKbCapture();
        }
    } else if (message == 0x1000b && a == 0x1b) {
        return Escape(window);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101ea734, 124 bytes: a finished download (error 0) sets the lobby's state to 2 and the game type to online.
void GeneralParkDownloadCompleteCallback(int32_t error, const void* context) {
    (void)context;
    if (error == 0) {
        gLobby->mState = 2;
        TheGameType().SetGameType(kGameTypeOnline);
    }
}

// 0x101ea684, 116 bytes: removes the panel, tells the caller's function (always set; not tested for NULL) and, on an error, shows the
// lobby's window again.
void VisitParkPostDownloadCallbackFn(int32_t error, const void* context) {
    (void)context;
    RemoveOnlineParkInfoPanel();
    gVisitParkPostDownload(error, gVisitParkContext);
    if (error != 0) UI_SendMessage(gLobby->mWindow, 6, 1, 0);
}

// 0x101ea5d8, 116 bytes: the two wide string globals of the file.
void __sinit_ui_onlineParkFindPanel_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* b = gOnlineParkFindStringB;
    void* a = gOnlineParkFindStringA;
    ::new (b) TbDynamicStringTemplate<char16_t>(kTextB);
    __register_global_object(b, dtor, gOnlineParkFindNodeB);
    ::new (a) TbDynamicStringTemplate<char16_t>(kTextA);
    __register_global_object(a, dtor, gOnlineParkFindNodeA);
}

// 0x101ea4e4, 192 bytes: when the find panel exists and its first entry is not yet a park (data -1), the entry's second column shows
// the progress text. The first column of the item is not set (the original passes whatever its stack held).
void UpdateParkFindPanelProgress(int32_t percent) {
    if (gOnlineParkFindPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    int32_t data = 0;
    list->GetData(0, &data);
    if (data == -1) {
        swprintf(gUIMainConvertBuffer, 0x3fff, kProgressFormat, percent);
        UI_LCAdd item[2];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[1].mText = gUIMainConvertBuffer;
        list->ChangeItem(0, item, 2, 1, -1);
    }
}

// 0x101ea468, 76 bytes: the advisor forgets its lines and the find panel is told to close.
void RemoveOnlineParkFindPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gOnlineParkFindPanel, 4, 0, 0);
}
