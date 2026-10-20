// The online panel's contact list helpers (unit v5660): the buddy list refill, the buttons menu's message function and the pop-up menu
// for a contact. Names invented.
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

namespace {

// The online control is made on first use (0xe8c bytes) at every call site of the original.
COnlineControl* OnlineControl() {
    if (gOnlineControl == nullptr) gOnlineControl = new COnlineControl();
    return gOnlineControl;
}

}  // namespace

// Empties the contacts list of the online panel and has the control refill it.
void FillBuddyList() {
    InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
    static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124))->RemoveAllItems();
    OnlineControl()->UpdateBuddyList();
}

// The buttons menu's message function: on a button press (0x100) the button's id picks the command on the name kept in extra
// parameter 0 (ids below 0x178a and between 0x1790 and 0x3003 do nothing), then the menu hides. Every message goes on to the default
// handler.
int32_t ButtonsMenuMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        std::u16string name(static_cast<const char16_t*>(window->getExtraParam(0)));
        switch (a) {
            case 0x178a:
            case 0x3004:
                OnlineControl()->BlackMark(name);
                break;
            case 0x178b:
            case 0x3005:
                OnlineControl()->Goto(name);
                break;
            case 0x178c:
            case 0x3006:
                if (b == 1)
                    OnlineControl()->IgnoreOn(name);
                else
                    OnlineControl()->IgnoreOff(name);
                break;
            case 0x178d:
                OnlineControl()->BuddyOff(name);
                break;
            case 0x178e:
                OnlineControl()->Locate(name);
                break;
            case 0x178f:
                OnlineControl()->Page(name);
                break;
            case 0x3007:
                OnlineControl()->BuddyOn(name);
                break;
            default:
                break;
        }
        HideButtonsMenu();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Pops the buttons menu up at the mouse for the contact at `position` of the contacts list (nothing for the player's own name). Bit 0 of
// the contact's data picks the menu (set: the one in gOnlineWindowE90, else gOnlineWindowE8c), bit 3 the state of its ignore button.
void ContactMenu(int32_t position) {
    InterfaceWindow* panel = gOnlinePanel->GetWindow(0xc3123);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->GetWindow(0xc3124));
    int32_t data[4];
    list->GetData(position, data);
    UI_LCAdd item;
    list->GetItem(position, 0, item);
    if (OnlineControl()->IsThisMe(item.mText)) return;
    InterfaceWindow* menu;
    InterfaceWindow* sub;
    InterfaceWindow* button;
    int32_t subId, buttonId;
    if (data[0] & 1) {
        gOnlineWord88 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOnlineWindowE90));
        subId = 0x1789;
        buttonId = 0x178c;
    } else {
        gOnlineWord88 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOnlineWindowE8c));
        subId = 0x3003;
        buttonId = 0x3006;
    }
    // The original reloads the menu from gOnlineWord88 (a window pointer in a word); the local copy is the same window.
    menu = (data[0] & 1) ? gOnlineWindowE90 : gOnlineWindowE8c;
    sub = menu->GetWindow(subId);
    gOnlinePanel->GetWindow(0xc3123)->GetWindow(0xc3124)->link(menu, 1, 1);
    {
        CoolRect rect;
        sub->GetPosition(&rect, 1);
        int16_t mouseX, mouseY;
        UI_GetMouseXY(mouseX, mouseY);
        sub->ShiftPosition(static_cast<int16_t>(mouseX - rect.mLeft), static_cast<int16_t>(mouseY - rect.mTop));
        menu->setZ(static_cast<int16_t>(menu->mZ + 4));
        menu->Show(1);
        menu->CaptureKb(0);
    }
    button = sub->GetWindow(buttonId);
    static_cast<InterfaceButton*>(button)->SetState((data[0] & 8) ? 1 : 0, 1);
    sub->setExtraParam(0, const_cast<char16_t*>(item.mText));
}
