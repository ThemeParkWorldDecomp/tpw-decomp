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
#include "Advisor.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "Text.h"
#include "ui_DrawObject.h"
#include "online.h"

int16_t gOnlineParkFindScript[2] = {0, 1};   // 0x102b0c8c (the rest of the script is not kept here)
// 0x1024f01e: empty wide strings, the cells of the find list point at +0 and +2 (bytes).
const char16_t gOnlineParkFindWide[4] = {0, 0, 0, 0};

#ifndef TPW_FINDPANEL_UNIT_TEST
// The callees the host cannot call as the Mac does: weak here, defined by the test library (which defines TPW_FINDPANEL_UNIT_TEST).
namespace findpanel_callee {
__attribute__((weak)) InterfaceDrawObject* NewCursorSprite() { return new InterfaceCursorPosTextObjectSprite(nullptr); }
__attribute__((weak)) CFSOnlineSystem* NewOnlineSystem() { return new CFSOnlineSystem(); }
__attribute__((weak)) const char16_t* LoginCustomerName(CFSOnlineSystem* system) { return online_callee::LoginCustomerName(system); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
}  // namespace findpanel_callee

// The find list's row functions (0x101e96b0, 0x101e9874) are not translated yet.
__attribute__((weak)) InterfaceWindow* OnlineParkFindLCFormat() { return nullptr; }
__attribute__((weak)) void OnlineParkFindLCPostFormat(InterfaceWindow*) {}
#endif

namespace {
char kHilight[] = "hilight";   // 0x1024efd8
}  // namespace

// 0x101e9f28, 164 bytes: the find list shows the "searching" row.
void ShowFindListSearching() {
    if (gOnlineParkFindPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    list->RemoveAllItems();
    UI_LCAdd item[6] = {};
    item[0].mText = UIMain_GetString(0x134);
    item[1].mText = reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gOnlineParkFindWide) + 2);
    item[2].mText = gOnlineParkFindWide;
    list->AddItem(item, -1, -1, 1);
}

// 0x101e9ff8, 1084 bytes: builds the park find panel (once) and fills in its edit box and list.
void CreateOnlineParkFindPanel(void (*onCancel)()) {
    if (gOnlineParkFindPanel != nullptr) return;
    gOnlineParkFindPanel = gLobby->mWindow->Build(gOnlineParkFindScript, OnlineParkFindPanelMsg);
    gFindPanelCloseCallback = onCancel;
    InterfaceEditBox* edit = static_cast<InterfaceEditBox*>(gOnlineParkFindPanel->GetWindow(0xe05a));
    InterfaceDrawObject* sprite = findpanel_callee::NewCursorSprite();
    edit->SetClientDrawObject(sprite);
    edit->SetFont(GetFontTableEntry(6));
    edit->SetStaticTextArea(gFindPanelPlayerName, 0xf);
    edit->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
    findpanel_callee::WindowSetText(edit, const_cast<char16_t*>(findpanel_callee::LoginCustomerName(gOnlineSystem)));
    edit->CaptureKb(0);

    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    list->SetColumnFormatsAll(OnlineParkFindLCFormat, OnlineParkFindLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCText);
    list->SetColumnType(2, kLCText);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetColumnType(5, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(kHilight));
    for (int32_t i = 0; i < 6; ++i) {
        const char16_t* text = UIMain_GetString(0x12c + i);
        InterfaceWindow* column = list->GetWindow(0x10 + i);
        MAKECOLUMNBUTTON(column, const_cast<char16_t*>(text), 8, 0xff, 0xef, 0, 0x17d + i);
    }
    UI_LCAdd item[6] = {};
    item[0].mText = UIMain_GetString(0x133);
    item[1].mText = gOnlineParkFindWide;
    item[2].mText = gOnlineParkFindWide;
    list->AddItem(item, 0, -1, 1);
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x12b);
    UIMain_SetTitleText(gOnlineParkFindPanel, 0xe05c, gUIMainConvertBuffer);
    AdvisorResponse response;
    response.mId = 0x254;
    gLobbyScreens->mAdvisor->Add(response, 1);
}
