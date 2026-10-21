// ui_allItemsPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "helpers/ui_allItemsPanelHelpers.h"
#include "helpers/ui_allStaffPanelHelpers.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>

// Unit v5219: the all-kids panel's creation and its list rows, and the rides / sideshows list rows (ui_allKidsPanel.cpp, ui_allStaffPanel.cpp):
//   CreateAllKidsPanel 0x101952bc, AllKidsPanel_KidAdded 0x101957b8, AddItemToAllRidesList 0x10195978,
//   FillAllSideshowsListCallback 0x10195dc8, UpdateRideListEntries 0x1019600c.
//
// The cells of a row are one word each on the Mac (a stack block); a person or a ride is reached by the Mac's offsets.
// Two runtime helpers are called as the listing does (no source names them): 0x10237cd8 turns the sum of a history (a double) into a
// word (the listing keeps r3, the HIGH word of a 64-bit conversion), 0x10237e1c divides a 64-bit span (r3:r4) by r5:r6 and the
// listing keeps the low word.

uint16_t gAllRidesNoParent = 0;         // 0x102a8d0a the upgrade-parent id UpdateRideListEntries treats as "no other ride" (0 at start)

namespace {

using ThingParent = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024de34

LCWord Text(const char16_t* text) {
    LCWord word;
    word.mRaw = 0;
    word.mText = text;
    return word;
}

// 0x10237cd8: the word the listing keeps from converting a history's sum (the high word of the 64-bit result).
int32_t SumWord(double sum) {
    double rounded = sum;
    if (!(rounded > 0.0)) return 0;
    if (rounded >= 4294967296.0) return -1;
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(rounded)));
}

}  // namespace

// 0x10195978 (368 bytes): adds the ride `thing` to the rides list, at the position of `object` when one is given (nothing when it is not in the list).
void AddItemToAllRidesList(const CThing* thing, InterfaceListControl2* list, CObject* object) {
    CObject* base = object != nullptr ? object : reinterpret_cast<CObject*>(const_cast<CThing*>(thing));
    base->GetDBInfo();
    UI_LCAdd cells[5];
    cells[0] = Text(UIMain_GetObjectName(reinterpret_cast<CObject*>(const_cast<CThing*>(thing))));
    double sum;
    base->mHistory1A4.GetSumOfLast(sum, 30);
    cells[1] = Number(SumWord(sum));
    cells[2] = Number(PercentOfByte(base->GetExcitementLevel(false)));
    cells[3] = Number(PercentOfByte(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(base->mF40)))));
    cells[4] = Number(PercentOfByte(static_cast<uint32_t>(base->GetRemainingLife())));
    if (object == nullptr) {
        list->AddItem(cells, DataOf(thing), -1, 0);
    } else {
        int32_t position = list->FindData(DataOf(object));
        if (position >= 0) list->AddItem(cells, DataOf(thing), position, 0);
    }
}

// Unit v51109: small stand-ins of the panels (removers, static initialisers, counters).
void RemoveAllKidsPanel();
void __sinit_ui_allKidsPanel_cpp();
void __sinit_ui_StatusPanel_cpp();
void __sinit_ui_allStaffPanel_cpp();
void __sinit_ui_textmanager_cpp();
void __sinit_ui_entryPricePanel_cpp();

// 0x10195b40 (48 bytes): the all-rides list's row: the thing goes to the list with no upgrade ride. Answers 1.
int32_t FillAllRidesListCallback(CThing* thing) {
    AddItemToAllRidesList(thing, gAllItemsList, nullptr);
    return 1;
}

// Unit v5952: FillAllRidesListCallback_Upgrades 0x10195ba8, FillAllShopsListCallback 0x10195cb0, FillAllFeaturesListCallback 0x10195f24,
// AllKidsPanel_KidGone 0x10195708 and the kids list's attach function 0x10194fa8 (AllKidsListLCPostFormat; the Mac name is LCPostFormat).

uint16_t gAllRidesNoUpgradeParent = 0;   // 0x102a8d08

namespace {

// 0x10237cd8: the double as an unsigned word (the sum of the history).
int32_t SumWord_2(double sum) {
    double rounded = sum;
    if (rounded != rounded) return -1;   // a NaN
    if (!(rounded > 0.0)) return 0;
    if (rounded >= 4294967296.0) return -1;
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(rounded)));
}

}  // namespace

// 0x10195ba8 (200 bytes): the upgrades list's row: the ride this one upgrades (the thing array's entry of its upgrade parent, or none when
// the parent is the "no parent" id) goes to AddItemToAllRidesList with the thing. The index is not range checked. Answers 1.
int32_t FillAllRidesListCallback_Upgrades(CThing* thing) {
    CObject* object = nullptr;
    CObject* self = reinterpret_cast<CObject*>(thing);
    if (self->GetUpgradeParent().mIndex != gAllRidesNoUpgradeParent) {
        uint16_t index = self->GetUpgradeParent().mIndex;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
        object = static_cast<CObject*>(gThingArray[index].mThing);
    }
    AddItemToAllRidesList(thing, gAllItemsList, object);
    return 1;
}

// 0x10195cb0 (224 bytes): one row for the shop. Answers 1.
int32_t FillAllShopsListCallback(CThing* thing) {
    InterfaceListControl2* list = gAllItemsList;
    CObject* object = reinterpret_cast<CObject*>(thing);
    object->GetDBInfo();
    UI_LCAdd cells[5];
    cells[0] = Text(UIMain_GetObjectName(object));
    double sum;
    object->mHistory1A4.GetSumOfLast(sum, 30);
    cells[1] = Number(SumWord_2(sum));
    cells[2] = Number(object->GetProfitability());
    cells[3] = Number(object->GetTotalRevenue());
    uint32_t satisfaction = static_cast<uint32_t>(object->GetCustomerSatisfaction());
    cells[4] = Number(Div100(static_cast<int32_t>(satisfaction << 10)));
    list->AddItem(cells, DataOf(thing), -1, 0);
    return 1;
}

// 0x10195dc8 (288 bytes): UI_IterateThingList's function for the sideshows list: one row for the sideshow. Answers 1.
int32_t FillAllSideshowsListCallback(CThing* thing) {
    InterfaceListControl2* list = gAllItemsList;
    CObject* object = reinterpret_cast<CObject*>(thing);
    object->GetDBInfo();
    UI_LCAdd cells[6];
    cells[0] = Text(UIMain_GetObjectName(object));
    double sum;
    object->mHistory1A4.GetSumOfLast(sum, 30);
    cells[1] = Number(SumWord(sum));
    cells[2] = Number(PercentOfByte(object->GetExcitementLevel(false)));
    cells[3] = Number(object->GetProfitability());
    cells[4] = Number(object->GetTotalRevenue());
    uint32_t satisfaction = static_cast<uint32_t>(object->GetCustomerSatisfaction());
    cells[5] = Number(Div100(static_cast<int32_t>(satisfaction << 10)));
    list->AddItem(cells, DataOf(thing), -1, 0);
    return 1;
}

// 0x10195f24 (172 bytes): one row for the feature: its name and how many of its object type the park holds. Answers 1.
int32_t FillAllFeaturesListCallback(CThing* thing) {
    InterfaceListControl2* list = gAllItemsList;
    CObject* object = reinterpret_cast<CObject*>(thing);
    object->GetDBInfo();
    UI_LCAdd cells[2];
    cells[0] = Text(UIMain_GetObjectName(object));
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = object->mObjectType;
    cells[1] = Number(gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id));
    list->AddItem(cells, DataOf(thing), -1, 0);
    return 1;
}

// 0x1019600c (440 bytes): rebuilds the cells of every row of the rides list, last row first (a row whose ride has an upgrade parent shows the
// parent's ride: the thing array entry of the parent).
void UpdateRideListEntries(InterfaceListControl2* list) {
    if (list == nullptr) return;
    for (int32_t position = list->mItemCount - 1; position >= 0; --position) {
        int32_t data;
        list->GetData(position, &data);
        Ptr32<CObject> pointer;
        pointer.mOffset = static_cast<uint32_t>(data);
        CObject* object = pointer.get();
        ThingParent parent = object->GetUpgradeParent();
        if (parent.mIndex != gAllRidesNoParent) {
            ThingParent again = object->GetUpgradeParent();
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
            object = static_cast<CObject*>(gThingArray[again.mIndex].mThing);
        }
        UI_LCAdd cells[5];
        cells[0] = Number(0);
        double sum;
        object->mHistory1A4.GetSumOfLast(sum, 30);
        cells[1] = Number(SumWord(sum));
        cells[2] = Number(PercentOfByte(object->GetExcitementLevel(false)));
        cells[3] = Number(PercentOfByte(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(object->mF40)))));
        cells[4] = Number(PercentOfByte(static_cast<uint32_t>(object->GetRemainingLife())));
        list->ChangeItem(position, cells, 30, position == 0 ? 1 : 0, -1);
    }
}

// Unit v5335: the sideshows list rows and the all-items list's row window.
//   UpdateSideshowListEntries 0x10196208, LCFormat 0x10196388 (here AllItemsListLCFormat: the name LCFormat exists at other addresses, the
//   all-kids panel's is in src/ui_allKidsPanel_AllKidsPanelMsg.cpp).

// Globals no other file names (names invented).
int32_t gAllItemsRowHeight;               // 0x103947d4 the row window's height, worked out once (0 until then)
uint32_t gAllItemsFontIndex = 7;          // 0x102a8cf0 the font table index the row height is measured with (7 at start)

namespace {

// The runtime's double to unsigned conversion (glue 0x10237cd8, __cvt_fp2unsigned): the word the listing keeps.
int32_t SumWord_3(double sum) {
    if (sum >= 2147483648.0) return static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(sum - 2147483648.0)) + 0x80000000u);
    return ppc_fctiwz(sum);
}

}  // namespace

// The virtual call through the font entry's object (slot 0xc of the object at +0x10): its class is not known, the font's height stands in
// for the answer. Weak so that the test library's recording fake replaces it.
namespace allitems_callee {
__attribute__((weak)) int16_t FontVirtual(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }
}  // namespace allitems_callee

// 0x10196208 (312 bytes): rebuilds the cells of every row of the sideshows list, last row first. The row's data word is the object.
void UpdateSideshowListEntries(InterfaceListControl2* list) {
    if (list == nullptr) return;
    for (int32_t position = list->mItemCount - 1; position >= 0; --position) {
        int32_t data;
        list->GetData(position, &data);
        Ptr32<CObject> pointer;
        pointer.mOffset = static_cast<uint32_t>(data);
        CObject* object = pointer.get();
        UI_LCAdd cells[6];
        cells[0] = Number(0);
        double sum;
        object->mHistory1A4.GetSumOfLast(sum, 30);
        cells[1] = Number(SumWord_3(sum));
        cells[2] = Number(PercentOfByte(object->GetExcitementLevel(false)));
        cells[3] = Number(object->GetProfitability());
        cells[4] = Number(object->GetTotalRevenue());
        int32_t satisfaction = object->GetCustomerSatisfaction();
        cells[5] = Number(Div100(static_cast<int32_t>(static_cast<uint32_t>(satisfaction) << 10)));
        list->ChangeItem(position, cells, 0x3e, position == 0 ? 1 : 0, -1);
    }
}

// 0x10196388 (836 bytes): makes the all-items list's row window; its height (once) is the font's height (a virtual call on the font
// entry's object) scaled by 0x600 over the real screen height, plus 6. The children depend on the kind of list shown.
InterfaceWindow* AllItemsListLCFormat() {
    if (gAllItemsRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gAllItemsFontIndex);
        int16_t height = allitems_callee::FontVirtual(font);
        gAllItemsRowHeight = (static_cast<int32_t>(height) * 0x600) / static_cast<int32_t>(gUIRealScreenHeight) + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (window != nullptr)
        lender_callee::WindowCtor(window, nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gAllItemsRowHeight));
    window->SetCallbackFunction(AllItemsLCEntryHandler);
    __asm__ volatile("" : "+r"(window));   // the listing tests the pointer again: keep the compiler from assuming it is not NULL
    if (window != nullptr) {
        switch (gAllItemsListType) {
            case 2:
                window->CreateObject(1, 1, 2, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 3, 0, 0, 0, 0);
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(1, 1, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                break;
            case 1:
                window->CreateObject(1, 1, 3, 0, 0, 0, 0);
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(1, 1, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                break;
            case 0:
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 3, 0, 0, 0, 0);
                break;
            case 3:
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                break;
            default:
                break;
        }
    }
    return window;
}

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

// Unit v5002: the all-items panel's message functions and the all-features panel (ui_allStaffPanel.cpp).
//   MainWindowCallback 0x10197c2c, ListTemplateCallback 0x10197dac, CreateAllFeaturesPanel 0x101979c0, ActualListCallback 0x10198064

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

// Unit v5066: the all-items panel's builder and entry handler, and the all-staff panel's list functions (ui_allStaffPanel.cpp).
//   CreateAllItemsPanel 0x101983c4, AllItemsLCEntryHandler 0x10198264, FillAllStaffListCallback 0x10198690,
//   FillAllStaffList 0x101987fc, UpdateListEntries 0x10198944

// ---- globals
int16_t gAllItemsScript[] = {TPW_WINDOW_SCRIPT_102A8884};   // 0x102a8884: the image's words (generated, devtools/gen_game_tables.py)
// 0x102a8d0c: the upgrade-parent id AllItemsLCEntryHandler compares with (0 at start; the word before gAllStaffScript)
uint16_t gAllItemsNoParent = 0;
// 0x10198264 (284 bytes): message 0x403 (an entry is drawn; b is the entry's object): the font colour is the colour of the object's
// state; an object with an upgrade parent shows the parent's state instead (the parent found in the thing array). Then the default handler.
int32_t AllItemsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        CObject* object = DataPointer<CObject>(b);
        if (object->GetUpgradeParent().mIndex != gAllItemsNoParent) {
            uint16_t parent = object->GetUpgradeParent().mIndex;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            object = static_cast<CObject*>(gThingArray[parent].mThing);
        }
        int32_t state = UIMain_GetObjectState(object, 2);
        uint32_t colour = UIMain_GetObjectStateTextColour(state);
        window->SetFontColour(static_cast<int32_t>(colour >> 24), static_cast<int32_t>((colour >> 16) & 0xff),
                              static_cast<int32_t>((colour >> 8) & 0xff), 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
// 0x101983c4 (556 bytes): builds the all-items panel for a kind of item (-1: the kind the last list showed), checks the kind's button,
// fills the list through the kind's builder, sets the title and the callback.
void CreateAllItemsPanel(UIObjType type, CThing* thing) {
    int32_t kind = static_cast<int32_t>(type);
    if (kind == -1) kind = gAllItemsListType;
    UIMain_CheckPanel();
    SetLastPanel(2, 5);
    gAllItemsPanel = gGameWindow->Build(gAllItemsScript, MainWindowCallback);
    UIMain_SetPanel(gAllItemsPanel);
    InterfaceWindow* group = gAllItemsPanel->GetWindow(0x12c4b8);
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(group->GetWindow(0x12c4b9));
    if (kind == 2) {
        buttons->SetCurrent(0x12c4bb);
        CreateAllShowsPanel(thing);
    } else if (kind < 2) {
        if (kind == 0) {
            buttons->SetCurrent(0x12c4ba);
            CreateAllRidesPanel(thing);
        } else if (kind >= 0) {
            buttons->SetCurrent(0x12c4bc);
            CreateAllShopsPanel(thing);
        }
    } else if (kind < 4) {
        buttons->SetCurrent(0x12c4bd);
        CreateAllFeaturesPanel(thing);
    }
    if (kind == 2) {
        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x50)));
    } else if (kind < 2) {
        if (kind == 0) {
            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4e)));
        } else if (kind >= 0) {
            UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x4f)));
        }
    } else if (kind < 4) {
        UIMain_SetTitleText(gAllItemsPanel, 0x12c4be, const_cast<char16_t*>(UIMain_GetString(0x51)));
    }
    gAllItemsPanel->GetWindow(0x12c4b8)->SetCallbackFunction(ListTemplateCallback);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xbf));
}

// 0x1019862c (56 bytes)
void RemoveAllItemsPanel() { UI_SendMessage(gAllItemsPanel, 4, 0, 0); }

// Unit v5560: the all-items list's row attach function (LCPostFormat of ui_allKidsPanel.cpp, 0x101966ec; named AllItemsListLCPostFormat
// because LCPostFormat exists at other addresses). The row gets a text draw object, then the columns that the list type needs.

namespace {

using namespace allitems_post_callee;

// The number of bytes the Mac allocates for a text object (0x19c) and for a gradient (0x124).
void* NewText() { return OperatorNew(0x19c); }

void SetFontOf(InterfaceWindow* window) { window->SetFont(GetFontTableEntry(gAllItemsFontIndex)); }

// A number column: number text, the font, the value cleared, centred.
void NumberColumn(InterfaceWindow* window) {
    void* memory = NewText();
    NumberCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(memory)));
    SetFontOf(window);
    WindowSetValue(window, 0);
    window->SetFontJust(1, 1);
}

// A cash column: cash text, right-aligned, the font, the value cleared.
void CashColumn(InterfaceWindow* window) {
    void* memory = NewText();
    CashCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashTextObject*>(memory)));
    window->SetFontJust(2, 1);
    SetFontOf(window);
    WindowSetValue(window, 0);
}

// A bar column: indented by 10, a horizontal gradient of the status bar's mesh, the value cleared.
void BarColumn(InterfaceWindow* window) {
    UIMain_IndentClient(window, 0, 10);
    void* memory = OperatorNew(0x124);
    GradientCtor(memory, "ridestatbar.wct");
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceDrawEngineHGradient*>(memory)));
    WindowSetValue(window, 0);
}

}  // namespace

// The callees the host cannot call as the Mac does. The test library defines TPW_LCPOST_FAKES (its cmake file) and brings recording fakes.
#ifndef TPW_LCPOST_FAKES
namespace allitems_post_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceDrawEngineHGradient) <= 0x400, "the allocation below is 0x400 at least");
void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
void NumberCtor(void* mem) { new (mem) InterfaceNumberTextObject(nullptr); }
void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
void GradientCtor(void* mem, const char* mesh) { new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(mesh)); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace allitems_post_callee
#endif

void AllItemsListLCPostFormat(InterfaceWindow* window) {
    void* memory = NewText();
    SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(memory)));
    SetFontOf(window);
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    switch (gAllItemsListType) {
    case 2:
        NumberColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 0:
        NumberColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 1:
        NumberColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 3: {
        void* number = NewText();
        NumberCtor(number);
        child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(number)));
        SetFontOf(child);
        WindowSetValue(child, 0);
        child->SetFontJust(1, 1);
        break;
    }
    default:
        break;
    }
}
