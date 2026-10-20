#include <cstdint>
#include "Ptr32.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "UI_Base.h"
#include "ui_iterators.h"

namespace {
// The original reads this from inside a string pool at 0x1024de34 + 0x53.
char kHilight[] = "hilight";
}  // namespace

// 0x10196ed0 (936 bytes): builds the all-sideshows list inside the all-items panel and puts the column buttons on it.
void CreateAllShowsPanel(CThing* thing) {
    InterfaceWindow* group = gAllItemsPanel->GetWindow(0x12c4b8);
    InterfaceWindow* target = group->GetWindow(400);
    if (target == nullptr) {
        target = group->GetWindow(400);
        if (target == nullptr) {
            target = group->GetWindow(400);
        }
    }
    if (target != nullptr) UI_SendMessage(target, 4, 0, 0);

    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(
        gAllItemsPanel->GetWindow(0x12c4b8)->Build(gAllShowsScript, nullptr));
    gAllItemsListType = 2;
    list->SetCallbackFunction(ActualListCallback);
    list->SetColumnFormatsAll(AllItemsListLCFormat, AllItemsListLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCNumber);
    list->SetColumnType(2, kLCNumber);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetColumnType(5, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(kHilight));
    list->SetSort(gAllShowsSort, false);
    gAllItemsList = list;
    UI_IterateThingList(kUIIterSideshows, FillAllSideshowsListCallback);
    list->UpdateDisplay();
    if (thing != nullptr) {
        int32_t found = list->FindData(static_cast<int32_t>(reinterpret_cast<uintptr_t>(thing)));
        if (found >= 0) list->SetCurrent(found);
    }
    static const int32_t kIds[6] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
    static const int32_t kStrings[6] = {0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61};
    static const int32_t kLast[6] = {0x69, 0x248, 0x6b, 0x249, 0x24a, 0x24b};
    for (int i = 0; i < 6; i++) {
        InterfaceWindow* button = list->GetWindow(kIds[i]);
        char16_t* text = const_cast<char16_t*>(UIMain_GetString(kStrings[i]));
        MAKECOLUMNBUTTON(button, text, gAllColumnButtonFont, 0xff, 0xef, 0, kLast[i]);
    }
}
