// Unit v5219: the all-kids panel's creation and its list rows, and the rides / sideshows list rows (ui_allKidsPanel.cpp, ui_allStaffPanel.cpp):
//   CreateAllKidsPanel 0x101952bc, AllKidsPanel_KidAdded 0x101957b8, AddItemToAllRidesList 0x10195978,
//   FillAllSideshowsListCallback 0x10195dc8, UpdateRideListEntries 0x1019600c.
//
// The cells of a row are one word each on the Mac (a stack block); a person or a ride is reached by the Mac's offsets.
// Two runtime helpers are called as the listing does (no source names them): 0x10237cd8 turns the sum of a history (a double) into a
// word (the listing keeps r3, the HIGH word of a 64-bit conversion), 0x10237e1c divides a 64-bit span (r3:r4) by r5:r6 and the
// listing keeps the low word.
#include <cstddef>
#include <cstdint>

#include "Debug.h"
#include "History.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "WindowScripts.inc"

// ---- globals first named here (no header, source or import names them)
int16_t gAllKidsScript[] = {TPW_WINDOW_SCRIPT_102A868C};   // 0x102a868c: the image's words (generated, devtools/gen_game_tables.py)
int32_t gAllKidsColumnFont = 8;         // 0x102a887c the font number of the column buttons
int32_t gAllKidsCounter = 0;            // 0x103947c0 cleared before the list is filled
uint16_t gAllRidesNoParent = 0;         // 0x102a8d0a the upgrade-parent id UpdateRideListEntries treats as "no other ride" (0 at start)

namespace {

using ThingParent = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024de34

int32_t DataOf(const void* pointer) { return pointer != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(pointer)) : 0; }

template <class T>
T& At(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

LCWord Number(int32_t value) {
    LCWord word;
    word.mRaw = 0;
    word.mInt = value;
    return word;
}

LCWord Text(const char16_t* text) {
    LCWord word;
    word.mRaw = 0;
    word.mText = text;
    return word;
}

// The listing's divide by 100 (mulhw by 0x51eb851f, srawi 5, plus the sign): the plain truncating division.
int32_t Div100(int32_t value) { return value / 100; }

// A byte (0 to 255) as the list's per-mille style cell: (byte << 10) / 100.
int32_t PercentOfByte(uint32_t value) { return Div100(static_cast<int32_t>((value & 0xffu) << 10)); }

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

// 0x101957b8 (320 bytes): a kid has come into the park: adds his row to the all-kids list when the panel is open.
void AllKidsPanel_KidAdded(CThing* thing) {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    gAllKidsListControl = list;
    UI_LCAdd cells[6];
    cells[0] = Number(At<int32_t>(thing, 0x1d8));
    cells[1] = Number(At<int32_t>(thing, 0x1a0));
    uint32_t turns = reinterpret_cast<CPerson*>(thing)->GetTimeInPark();
    const CParkTime* parkTime = reinterpret_cast<const CParkTime*>(reinterpret_cast<const uint8_t*>(gGameWorld) + 0x2a0);
    TbTimeDiff span = parkTime->GetFunnyTimeForGameTurns(turns);
    cells[2] = Number(static_cast<int32_t>(span.mTicks / 0x861c46800LL));   // 0x10237e1c: hours (a signed 64-bit divide), the low word
    cells[3] = Number(At<int32_t>(thing, 0x1c4));
    int32_t stay = At<int32_t>(thing, 0x30);
    cells[4] = Number(stay == 0 ? 999 : stay - 1);
    cells[5] = Number(PercentOfByte(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(At<float>(thing, 0x19c))))));
    list->AddItem(cells, DataOf(thing), -1, 0);
    list->UpdateDisplay();
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

// 0x101952bc (952 bytes): opens the all-kids panel, fills its list with the visitors, selects `thing` when it is in it and labels the column buttons.
void CreateAllKidsPanel(CThing* thing) {
    UIMain_CheckPanel();
    SetLastPanel(2, 6);
    InterfaceWindow* built = gGameWindow->Build(gAllKidsScript, AllKidsPanelMsg);
    if (built == nullptr) return;
    gAllKidsMsgWindow = built;
    UIMain_SetPanel(gAllKidsMsgWindow);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    if (list != nullptr) {
        list->SetColumnFormatsAll(LCFormat, AllKidsListLCPostFormat);
        for (int16_t column = 0; column < 6; ++column) list->SetColumnType(column, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        list->SetSort(gAllKidsSelectedIndex, false);
        InterfaceListControl2* filled = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
        gAllKidsListControl = filled;
        gAllKidsCounter = 0;
        UI_IterateThingList(kUIIterVisitors, FillAllKidsListCallback);
        filled->UpdateDisplay();
        if (thing != nullptr) {
            int32_t position = list->FindData(DataOf(thing));
            if (position >= 0) list->SetCurrent(position);
        }
        for (int32_t i = 0; i < 6; ++i) {
            InterfaceWindow* button = list->GetWindow(0x10 + i);
            char16_t* text = const_cast<char16_t*>(UIMain_GetString(0x71 + i));
            MAKECOLUMNBUTTON(button, text, gAllKidsColumnFont, 0xff, 0xef, 0, 0x81 + i);
        }
    }
    if (gAllKidsMsgWindow->setAnimTimer(0x80083, 2000) != 0) gAllKidsMsgWindow->startAnimTimer(0x80083);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11e));
}
