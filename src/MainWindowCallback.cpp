// Unit v5002: the all-items panel's message functions and the all-features panel (ui_allStaffPanel.cpp).
//   MainWindowCallback 0x10197c2c, ListTemplateCallback 0x10197dac, CreateAllFeaturesPanel 0x101979c0, ActualListCallback 0x10198064
#include "MainWindowCallback.h"

#include "platform/arena.h"
#include "History.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "WindowScripts.inc"

// The data: the 32-bit words the Mac's data segment holds. gAllFeaturesScript is the window script (only its address is used here).
InterfaceWindow* gAllItemsPanel = nullptr;
int32_t gAllItemsListType = 0;
InterfaceListControl2* gAllItemsList = nullptr;
int32_t gAllRidesSort = 1;
int32_t gAllShopsSort = 1;
int32_t gAllShowsSort = 1;
int32_t gAllFeaturesSort = 1;
int32_t gAllColumnButtonFont = 8;
int16_t gAllFeaturesScript[] = {TPW_WINDOW_SCRIPT_102A8C3C};   // 0x102a8c3c: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// A pointer the list control keeps as a 32-bit data word (an offset into the arena on the host; 0 is NULL).
template <class T>
T* DataPointer(int32_t data) {
    return data != 0 ? reinterpret_cast<T*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr;
}

}  // namespace

// 0x10197c2c (320 bytes): the panel's messages. 5 removes the panel (answers 1). The window being destroyed (0x14) forgets the panel and
// cancels advisor message 0xbf, then the default. A button (0x100): -2 removes the panel, 0x12c4bf the park status panel, 0x12c4c0 all
// staff, 0x12c4c1 all kids (each answers 0). Everything else is the panels' default message function.
int32_t MainWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x15) {
        if (message < 0x15) {
            if (message == 5) {
                RemoveAllItemsPanel();
                return 1;
            }
            if (message > 5 && message >= 0x14) {
                gAllItemsPanel = nullptr;
                UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xbf));
            }
        } else if (message == 0x100) {
            if (a == 0x12c4bf) {
                CreateParkStatusPanel();
                return 0;
            }
            if (a < 0x12c4bf) {
                if (a == -2) {
                    RemoveAllItemsPanel();
                    return 0;
                }
            } else {
                if (a == 0x12c4c1) {
                    CreateAllKidsPanel(nullptr);
                    return 0;
                }
                if (a < 0x12c4c1) {
                    CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr);
                    return 0;
                }
            }
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x10197dac (632 bytes): the list templates' messages. 0x400 (a list item chosen): that item's thing's status panel (answers 0). 0x402:
// zoom to the item's thing and remove the panel (answers 0). 0x406 stores b in the sort column of the current list type, then the default.
// A button (0x101) with b 1: 0x12c4ba, bb, bc, bd open the rides, shows, shops and features list and set the title text.
int32_t ListTemplateCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x401) {
        if (message < 0x401) {
            if (message == 0x101) {
                if (b == 1) {
                    if (a == 0x12c4bc) {
                        CreateAllShopsPanel(nullptr);
                        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4f)));
                    } else if (a < 0x12c4bc) {
                        if (a == 0x12c4ba) {
                            CreateAllRidesPanel(nullptr);
                            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4e)));
                        } else if (a > 0x12c4ba) {
                            CreateAllShowsPanel(nullptr);
                            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x50)));
                        }
                    } else if (a < 0x12c4be) {
                        CreateAllFeaturesPanel(nullptr);
                        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x51)));
                    }
                }
            } else if (message == 0x400) {
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
                if (list != nullptr && b != -1) {
                    int32_t data = 0;
                    list->GetData(b, &data);
                    UIMain_GotoStatusPanelForThing(DataPointer<CThing>(data));
                }
                return 0;
            }
        } else if (message == 0x406) {
            switch (gAllItemsListType) {
                case 0: gAllRidesSort = b; break;
                case 1: gAllShopsSort = b; break;
                case 2: gAllShowsSort = b; break;
                case 3: gAllFeaturesSort = b; break;
                default: break;
            }
        } else if (message < 0x406 && message < 0x403) {   // 0x402
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
            if (list != nullptr && b != -1) {
                int32_t data = 0;
                list->GetData(b, &data);
                UIMain_ZoomToThing(DataPointer<CThing>(data)->GetThingId());
                RemoveAllItemsPanel();
                return 0;
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101979c0 (568 bytes): builds the all-features list in the all-items panel (window 0x12c4b8): empties the old list (message 4 to the
// window 400), builds the new one, sorts it, fills it from the features, selects `thing` and makes the two column buttons.
void CreateAllFeaturesPanel(CThing* thing) {
    InterfaceWindow* panel = gAllItemsPanel->GetWindow(0x12c4b8);
    InterfaceWindow* old = panel->GetWindow(400);
    if (old == nullptr) {
        old = panel->GetWindow(400);
        if (old == nullptr) {
            old = panel->GetWindow(400);
        }
    }
    if (old != nullptr) UI_SendMessage(old, 4, 0, 0);

    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllItemsPanel->GetWindow(0x12c4b8)->Build(gAllFeaturesScript, nullptr));
    gAllItemsListType = 3;
    list->SetCallbackFunction(ActualListCallback);
    list->SetColumnFormatsAll(AllItemsListLCFormat, AllItemsListLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    list->SetSort(gAllFeaturesSort, false);
    gAllItemsList = list;
    UI_IterateThingList(static_cast<UIIteratorTypes>(0x400), FillAllFeaturesListCallback);
    list->UpdateDisplay();
    if (thing != nullptr) {
        int32_t position = list->FindData(static_cast<int32_t>(tpw::ArenaOffset(thing)));
        if (position >= 0) list->SetCurrent(position);
    }
    InterfaceWindow* button = list->GetWindow(0x10);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x62)), gAllColumnButtonFont, 0xff, 0xef, 0, 0x69);
    button = list->GetWindow(0x11);
    MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x63)), gAllColumnButtonFont, 0xff, 0xef, 0, 0x24c);
}

// 0x10198064 (448 bytes): the list's own messages. 0x15 (shown): starts the refresh timer 0x80083 (2000 ms). The timer (0x10, id 0x80083):
// by list type the rides' or sideshows' entries are refreshed; a type 1 list has each item's four cells rewritten (the customer sum of
// the last 30, the profit, the revenue and the satisfaction in percent); the other types do nothing. Then the default handler.
int32_t ActualListCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        if (window->setAnimTimer(0x80083, 2000) != 0) window->startAnimTimer(0x80083);
    } else if (message < 0x15 && message == 0x10 && a == 0x80083) {
        int32_t type = gAllItemsListType;
        if (type == 2) {
            UpdateSideshowListEntries(static_cast<InterfaceListControl2*>(window));
        } else if (type < 2) {
            if (type == 0) {
                UpdateRideListEntries(static_cast<InterfaceListControl2*>(window));
            } else if (type >= 0 && window != nullptr) {
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
                for (int32_t i = list->mItemCount - 1; i >= 0; --i) {
                    int32_t data = 0;
                    list->GetData(i, &data);
                    CObject* object = DataPointer<CObject>(data);
                    UI_LCAdd item[5];
                    item[0].mRaw = 0;
                    item[0].mInt = 0;
                    double sum = 0.0;
                    object->mHistory1A4.GetSumOfLast(sum, 30);
                    item[1].mRaw = 0;
                    item[1].mInt = static_cast<int32_t>(static_cast<int64_t>(sum));
                    item[2].mRaw = 0;
                    item[2].mInt = object->GetProfitability();
                    item[3].mRaw = 0;
                    item[3].mInt = object->GetTotalRevenue();
                    // the satisfaction times 1024, divided by 100 by the multiply-high trick (0x51eb851f)
                    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(object->GetCustomerSatisfaction()) << 10);
                    int32_t quotient = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * scaled) >> 32) >> 5;
                    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
                    item[4].mRaw = 0;
                    item[4].mInt = quotient;
                    list->ChangeItem(i, item, 30, i == 0 ? 1 : 0, -1);
                }
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
