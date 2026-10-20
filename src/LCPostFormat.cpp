// Unit v5004: the staff room panel's list control formatter and message functions (main app, ui_staffRoomStatus.cpp / ui_onlineParkListPanel.cpp):
//   LCPostFormat 0x101c346c, StaffRoomPanelLCHandler 0x101c35a4, StaffRoomStatusMsg 0x101c3c68.
#include <cstdint>

#include "Ptr32.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

// 0x101c346c: gives the list control a text sprite as client draw object with the table font, then its first child (indented) a horizontal
// gradient ("ridestatbar.wct") and a SetValue(0) (the virtual at the table's +0x24).
void LCPostFormat(InterfaceWindow* window) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(gLCFontIndex));
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    UIMain_IndentClient(child, 0, 10);
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("ridestatbar.wct"));
    child->SetClientDrawObject(gradient);
    child->SetValue(0);
}

// 0x101c35a4: message 0x101 with b == 1 fills the staff room list for the type the control number (a) names.
int32_t StaffRoomPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffRoomStatusPanel* panel = &gStaffRoomStatusPanel;
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x1e7d: panel->FillStaffRoomList(static_cast<StaffType>(0), 1); break;
            case 0x1e7e: panel->FillStaffRoomList(static_cast<StaffType>(4), 1); break;
            case 0x1e7f: panel->FillStaffRoomList(static_cast<StaffType>(2), 1); break;
            case 0x1e80: panel->FillStaffRoomList(static_cast<StaffType>(1), 1); break;
            case 0x1e81: panel->FillStaffRoomList(static_cast<StaffType>(3), 1); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101c3c68: the staff room status panel's message function.
int32_t StaffRoomStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffRoomStatusPanel* panel = &gStaffRoomStatusPanel;
    switch (message) {
        case 5:
            panel->Remove();
            return 1;
        case 0x10:
            if (a == 0x80080) {
                // Rewrites column 1 of every row: the staff member's float at +0x1f8, cut to a byte times 1024 over 100.
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->mWindow->GetWindow(0x1e7b));
                for (int32_t i = list->mItemCount - 1; i >= 0; i--) {
                    int32_t data = 0;
                    list->GetData(i, &data);
                    // The data word is a 32-bit pointer (an offset in the arena, Ptr32) to the object whose float sits at +0x1f8.
                    Ptr32<uint8_t> object;
                    object.mOffset = static_cast<uint32_t>(data);
                    float value = *reinterpret_cast<float*>(object.get() + 0x1f8);
                    int32_t whole = static_cast<int32_t>(value);
                    UI_LCAdd item[2];
                    item[0].mRaw = 0;   // the original leaves this word as it was on the stack; column 0 is not rewritten
                    item[1].mRaw = 0;
                    item[1].mInt = static_cast<int32_t>((static_cast<uint32_t>(whole) & 0xffu) << 10) / 100;
                    list->ChangeItem(i, item, 2, i == 0, -1);
                }
            }
            break;
        case 0x14:
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(300));
            break;
        case 0x15:
            if (window->setAnimTimer(0x80080, 2000) != 0) window->startAnimTimer(0x80080);
            break;
        case 0x100:
            switch (a) {
                case -1:
                    panel->Remove();
                    return 0;
                case 0x1e7a:
                    panel->EraseObject();
                    return 0;
                case 0x1e83:
                    CreateAllStaffPanel(gStaffRoomStaffType, nullptr);
                    return 0;
                case 0x1e85:
                    panel->GetPreviousObject();
                    break;
                case 0x1e86:
                    panel->GetNextObject();
                    break;
                case 0x1e87:
                    panel->Move();
                    return 0;
                case 0x1e88:
                    CreateAllItemsPanel(static_cast<UIObjType>(3), gStaffRoomPanelThing);
                    return 0;
                default:
                    break;
            }
            break;
        case 0x400: {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr && b != -1) {
                int32_t data = 0;
                static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                Ptr32<CThing> thing;
                thing.mOffset = static_cast<uint32_t>(data);
                gStaffStatusPanel.Show(thing.get());
                return 0;
            }
            return 0;
        }
        case 0x401: {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr) {
                if (b == -1) {
                    if (panel->mScript != 0) {
                        SCR_KillScript(panel->mScript);
                        panel->mScript = 0;
                        panel->mF28 = -1;
                        panel->mF2c = -1;
                    }
                } else {
                    int32_t data = 0;
                    static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                    Ptr32<CStaff> staff;
                    staff.mOffset = static_cast<uint32_t>(data);
                    panel->UpdateViewSprite(staff.get());
                }
            }
            break;
        }
        case 0x406:
            gStaffRoomMsg406 = b;
            break;
        default:
            break;
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}
