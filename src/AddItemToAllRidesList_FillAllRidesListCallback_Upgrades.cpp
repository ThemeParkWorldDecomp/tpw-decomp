// Unit v5952: FillAllRidesListCallback_Upgrades 0x10195ba8, FillAllShopsListCallback 0x10195cb0, FillAllFeaturesListCallback 0x10195f24,
// AllKidsPanel_KidGone 0x10195708 and the kids list's attach function 0x10194fa8 (AllKidsListLCPostFormat; the Mac name is LCPostFormat).
#include <cstddef>
#include <cstdint>
#include <new>

#include "Debug.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

uint16_t gAllRidesNoUpgradeParent = 0;   // 0x102a8d08

namespace {

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024de34

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

int32_t DataOf(const void* pointer) { return pointer != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(pointer)) : 0; }

// 0x10237cd8: the double as an unsigned word (the sum of the history).
int32_t SumWord(double sum) {
    double rounded = sum;
    if (rounded != rounded) return -1;   // a NaN
    if (!(rounded > 0.0)) return 0;
    if (rounded >= 4294967296.0) return -1;
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(rounded)));
}

int32_t Div100(int32_t value) { return value / 100; }

char kKidNames[] = "ti_hungry\0happygrad.wct";   // 0x1024de14, the second name at +10

}  // namespace

namespace kidlist_callee {
__attribute__((weak)) void VisitorNumCtor(void* mem) { lender_callee::SpriteCtor(mem); }
// The stack rectangle is never read by the function: its constructor and destructor have no effect that is observable here.
__attribute__((weak)) void RectCtor(void*) {}
__attribute__((weak)) void RectDtor(void*) {}
}  // namespace kidlist_callee

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
    cells[1] = Number(SumWord(sum));
    cells[2] = Number(object->GetProfitability());
    cells[3] = Number(object->GetTotalRevenue());
    uint32_t satisfaction = static_cast<uint32_t>(object->GetCustomerSatisfaction());
    cells[4] = Number(Div100(static_cast<int32_t>(satisfaction << 10)));
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

// 0x10195708 (124 bytes): takes the kid's row out of the kids list (when the panel is open and the row is there) and counts it off.
void AllKidsPanel_KidGone(CThing* thing) {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    int32_t position = list->FindData(DataOf(thing));
    if (position < 0) return;
    list->RemoveItem(position);
    gAllKidsCounter = gAllKidsCounter - 1;
}

// 0x10194fa8 (736 bytes): the kids list row's columns: a visitor number, cash, a visitor time, a number, then two pictures (the second a
// gradient). The objects come from the allocator, which is checked for NULL like the Mac does.
void AllKidsListLCPostFormat(InterfaceWindow* window) {
    using namespace allitems_post_callee;
    alignas(16) unsigned char rect[sizeof(CoolRect) + 16];
    kidlist_callee::RectCtor(rect);
    void* memory = OperatorNew(0x19c);
    if (memory != nullptr) kidlist_callee::VisitorNumCtor(memory);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    window->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    window->SetFontJust(1, 1);
    InterfaceWindow* child = window->GetChild(0);
    memory = OperatorNew(0x19c);
    if (memory != nullptr) CashCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    WindowSetValue(child, 0);
    child->SetFontJust(2, 1);
    child = child->GetNext();
    memory = OperatorNew(0x19c);
    if (memory != nullptr) vsp_callee::TimeCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    child->SetFontJust(1, 1);
    child = child->GetNext();
    memory = OperatorNew(0x19c);
    if (memory != nullptr) NumberCtor(memory);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    child->SetFont(GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex)));
    WindowSetValue(child, 0);
    child = child->GetNext();
    UIMain_IndentClient(child, 4, 0);
    child->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(kKidNames));
    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    memory = OperatorNew(0x124);
    if (memory != nullptr) GradientCtor(memory, kKidNames + 10);
    child->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(memory));
    kidlist_callee::RectDtor(rect);
}
