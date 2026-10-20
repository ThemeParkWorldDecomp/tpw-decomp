#include "MainWindowCallback.h"
#include "platform/arena.h"
#include "History.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "WindowScripts.inc"

int16_t gAllRidesScript[] = {TPW_WINDOW_SCRIPT_102A8A5A};   // 0x102a8a5a: the image's words (generated, devtools/gen_game_tables.py)

namespace {
char kHilightName[] = "hilight";

void EmptyOld(InterfaceWindow* panel) {
    InterfaceWindow* old = panel->GetWindow(400);
    if (old == nullptr) {
        old = panel->GetWindow(400);
        if (old == nullptr) {
            old = panel->GetWindow(400);
        }
    }
    if (old != nullptr) UI_SendMessage(old, 4, 0, 0);
}

InterfaceListControl2* BuildList(int32_t type) {
    InterfaceListControl2* list =
        static_cast<InterfaceListControl2*>(gAllItemsPanel->GetWindow(0x12c4b8)->Build(gAllRidesScript, nullptr));
    gAllItemsListType = type;
    list->SetCallbackFunction(ActualListCallback);
    list->SetColumnFormatsAll(AllItemsListLCFormat, AllItemsListLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    list->SetColumnType(2, kLCNumber);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(kHilightName));
    return list;
}

void Finish(InterfaceListControl2* list, CThing* thing) {
    list->UpdateDisplay();
    if (thing != nullptr) {
        int32_t position = list->FindData(static_cast<int32_t>(tpw::ArenaOffset(thing)));
        if (position >= 0) list->SetCurrent(position);
    }
}

void Buttons(InterfaceListControl2* list, int32_t firstString, const int32_t* last) {
    for (int i = 0; i < 5; i++) {
        InterfaceWindow* button = list->GetWindow(0x10 + i);
        char16_t* text = const_cast<char16_t*>(UIMain_GetString(firstString + i));
        MAKECOLUMNBUTTON(button, text, gAllColumnButtonFont, 0xff, 0xef, 0, last[i]);
    }
}
}  // namespace

// 0x1019762c (864 bytes)
void CreateAllRidesPanel(CThing* thing) {
    EmptyOld(gAllItemsPanel->GetWindow(0x12c4b8));
    InterfaceListControl2* list = BuildList(0);
    list->SetSort(gAllRidesSort, false);
    gAllItemsList = list;
    UI_IterateThingList(kUIIterRides, FillAllRidesListCallback);
    UI_IterateThingList(kUIIterUpgrades, FillAllRidesListCallback_Upgrades);
    Finish(list, thing);
    static const int32_t kLast[5] = {0x69, 0x6a, 0x6b, 0x6c, 0x6d};
    Buttons(list, 0x52, kLast);
}

// 0x101972ac (844 bytes)
void CreateAllShopsPanel(CThing* thing) {
    EmptyOld(gAllItemsPanel->GetWindow(0x12c4b8));
    InterfaceListControl2* list = BuildList(1);
    list->SetSort(gAllShopsSort, false);
    gAllItemsList = list;
    UI_IterateThingList(kUIIterShops, FillAllShopsListCallback);
    Finish(list, thing);
    static const int32_t kLast[5] = {0x69, 0x248, 0x249, 0x24a, 0x24b};
    Buttons(list, 0x57, kLast);
}
