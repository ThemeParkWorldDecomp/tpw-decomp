// Test library for the all-kids panel's creation and rows and the rides / sideshows list rows (unit v5219; libtpw_unit_AddItemToAllRidesList.dylib,
// tests/test_AddItemToAllRidesList.py), main app:
//   src/AddItemToAllRidesList.cpp   CreateAllKidsPanel, AllKidsPanel_KidAdded, AddItemToAllRidesList, FillAllSideshowsListCallback, UpdateRideListEntries
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other files
// the unit reads or writes are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "History.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gGameWindow;                  // 0x103949e4
CWorld* gGameWorld;                            // 0x1037ebfc
CThingArrayEntry gThingArray[kThingArraySize]; // 0x1034cbfc
InterfaceListControl2* gAllKidsListControl;    // 0x103947c4
InterfaceWindow* gAllKidsMsgWindow;            // 0x103947c8
InterfaceListControl2* gAllItemsList;          // 0x103947d8
int32_t gAllKidsSelectedIndex = 1;             // 0x102a8880

namespace {

template <class T>
int64_t I(T value) {
    return static_cast<int64_t>(value);
}

int64_t U32(const void* pointer) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer))); }

void* FromOffset(int32_t offset) {
    Ptr32<char> pointer;
    pointer.mOffset = static_cast<uint32_t>(offset);
    return pointer.get();
}

void* PointerOf(int64_t value) { return reinterpret_cast<void*>(static_cast<uintptr_t>(value)); }

using ThingParent = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

const char kExpectedMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

// The cells of an AddItem or ChangeItem: the column count is the case's (the Mac's stack block is that long).
void LogItem(const char* name, const void* self, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, bool six, const UI_LCAdd* cells,
             int32_t count) {
    int64_t w[6] = {0, 0, 0, 0, 0, 0};
    for (int32_t i = 0; i < count && i < 6; ++i) w[i] = I(cells[i].mInt);
    (void)six;
    (void)self;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)name;
    (void)w;
}

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}

void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {I(type), I(panel)}); }

void UIMain_SetPanel(InterfaceWindow* window) { ev("UIMain_SetPanel", {lab(window)}); }

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {I(id)});
    return static_cast<const char16_t*>(PointerOf(ret("UIMain_GetString")));
}

void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), U32(text), I(a), I(b), I(c), I(d), I(e)});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {I(message)}); }

char16_t* UIMain_GetObjectName(CObject* object) {
    ev("UIMain_GetObjectName", {lab(object)});
    return static_cast<char16_t*>(PointerOf(ret("UIMain_GetObjectName")));
}

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*function)(CThing*)) {
    ev("UI_IterateThingList", {I(mask), lab(reinterpret_cast<const void*>(function))});
    return 0;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {I(condition), I(std::strcmp(format, kExpectedMessage) == 0 ? 1 : 0)});
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {I(std::strcmp(name, "hilight") == 0 ? 1 : 0)});
    return static_cast<InterfaceDrawObject*>(PointerOf(ret("FindDrawObject")));
}

// ---- callbacks whose addresses are passed (the real ones are in other files)
InterfaceWindow* LCFormat() { return nullptr; }
void AllKidsListLCPostFormat(InterfaceWindow*) {}
int32_t FillAllKidsListCallback(CThing*) { return 0; }
int32_t AllKidsPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- InterfaceWindow
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(PointerOf(ret("Build")));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), I(id)});
    const char* key = "GetWindow_other";
    switch (id) {
        case 0x1e497: key = "GetWindow_list"; break;
        case 0x10: key = "GetWindow_b0"; break;
        case 0x11: key = "GetWindow_b1"; break;
        case 0x12: key = "GetWindow_b2"; break;
        case 0x13: key = "GetWindow_b3"; break;
        case 0x14: key = "GetWindow_b4"; break;
        case 0x15: key = "GetWindow_b5"; break;
        default: break;
    }
    return static_cast<InterfaceWindow*>(PointerOf(ret(key)));
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), I(id), I(ms)});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), I(which)});
    return 0;
}

// ---- InterfaceListControl2
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), lab(reinterpret_cast<const void*>(makeRow)), lab(reinterpret_cast<const void*>(attach))});
}

void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {lab(this), I(column), I(type)}); }

void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), U32(object)}); }

void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), I(column), I(descending ? 1 : 0)}); }

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), lab(FromOffset(data))});
    return static_cast<int32_t>(ret("FindData"));
}

void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), I(position)}); }

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), I(position)});
    void* object = PointerOf(ret("GetDataData"));
    if (data != nullptr) *data = object != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(object)) : 0;
    return static_cast<int32_t>(ret("GetData"));
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    const void* object = FromOffset(data);
    const int32_t n = static_cast<int32_t>(ret("ncols"));
    const int64_t c[6] = {I(item[0].mInt), I(item[1].mInt), I(item[2].mInt), I(item[3].mInt), I(item[4].mInt), n > 5 ? I(item[5].mInt) : 0};
    if (n == 6)
        ev("AddItem", {lab(this), lab(object), I(position), I(update), c[0], c[1], c[2], c[3], c[4], c[5]});
    else
        ev("AddItem", {lab(this), lab(object), I(position), I(update), c[0], c[1], c[2], c[3], c[4]});
    return static_cast<int32_t>(ret("AddItem"));
}

void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {lab(this), I(position), I(columns), I(update), I(unused), I(item[0].mInt), I(item[1].mInt), I(item[2].mInt),
                      I(item[3].mInt), I(item[4].mInt)});
}

// ---- CObject, CPerson, CParkTime, CHistory
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return nullptr;
}

uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {lab(this), I(flag ? 1 : 0)});
    return static_cast<uint8_t>(ret("GetExcitementLevel"));
}

int32_t CObject::GetRemainingLife() const {
    ev("GetRemainingLife", {lab(this)});
    return static_cast<int32_t>(ret("GetRemainingLife"));
}

int32_t CObject::GetProfitability() const {
    ev("GetProfitability", {lab(this)});
    return static_cast<int32_t>(ret("GetProfitability"));
}

int32_t CObject::GetTotalRevenue() const {
    ev("GetTotalRevenue", {lab(this)});
    return static_cast<int32_t>(ret("GetTotalRevenue"));
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {lab(this)});
    return static_cast<int32_t>(ret("GetCustomerSatisfaction"));
}

ThingParent CObject::GetUpgradeParent() const {
    ev("GetUpgradeParent", {lab(this)});
    ThingParent parent{};
    parent.mIndex = static_cast<uint16_t>(ret("GetUpgradeParent"));
    return parent;
}

uint32_t CPerson::GetTimeInPark() const {
    ev("GetTimeInPark", {lab(this)});
    return static_cast<uint32_t>(ret("GetTimeInPark"));
}

TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    ev("GetFunnyTimeForGameTurns", {lab(this), I(turns)});
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>((static_cast<uint64_t>(ret("funny_hi")) << 32) | static_cast<uint32_t>(ret("funny_lo")));
    return span;
}

template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    ev("GetSumOfLast", {lab(reinterpret_cast<const char*>(this) - 0x1a4), I(count)});
    uint64_t bits = (static_cast<uint64_t>(ret("sum_hi")) << 32) | static_cast<uint32_t>(ret("sum_lo"));
    std::memcpy(&sum, &bits, 8);
    return 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_av_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameWindow", &gGameWindow},
        {"gGameWorld", &gGameWorld},
        {"gAllKidsListControl", &gAllKidsListControl},
        {"gAllKidsMsgWindow", &gAllKidsMsgWindow},
        {"gAllItemsList", &gAllItemsList},
        {"gAllKidsSelectedIndex", &gAllKidsSelectedIndex},
        {"gAllKidsColumnFont", &gAllKidsColumnFont},
        {"gAllKidsCounter", &gAllKidsCounter},
        {"gAllRidesNoParent", &gAllRidesNoParent},
        {"script", gAllKidsScript},
        {"cbMsg", reinterpret_cast<const void*>(&AllKidsPanelMsg)},
        {"cbFormat", reinterpret_cast<const void*>(&LCFormat)},
        {"cbPost", reinterpret_cast<const void*>(&AllKidsListLCPostFormat)},
        {"cbFill", reinterpret_cast<const void*>(&FillAllKidsListCallback)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_av_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module relies on (HOST_LAYOUT): the first two are the Mac's, the third is where the list keeps its entry count.
int32_t tpw_av_layout(int64_t* out) {
    alignas(16) static char block[0x1000];
    int32_t n = 0;
    CObject* object = reinterpret_cast<CObject*>(block);
    InterfaceListControl2* list = reinterpret_cast<InterfaceListControl2*>(block);
    out[n++] = reinterpret_cast<char*>(&object->mF40) - block;
    out[n++] = reinterpret_cast<char*>(&object->mHistory1A4) - block;
    out[n++] = reinterpret_cast<char*>(&list->mItemCount) - block;
    return n;
}

void tpw_av_set_thing(uint32_t index, uint64_t pointer) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)); }

// ---- the functions, one wrapper each with C types only
void tpw_av_add(uint64_t thing, uint64_t list, uint64_t object) {
    AddItemToAllRidesList(reinterpret_cast<const CThing*>(thing), reinterpret_cast<InterfaceListControl2*>(list), reinterpret_cast<CObject*>(object));
}
void tpw_av_kid(uint64_t thing) { AllKidsPanel_KidAdded(reinterpret_cast<CThing*>(thing)); }
int32_t tpw_av_fill(uint64_t thing) { return FillAllSideshowsListCallback(reinterpret_cast<CThing*>(thing)); }
void tpw_av_upd(uint64_t list) { UpdateRideListEntries(reinterpret_cast<InterfaceListControl2*>(list)); }
void tpw_av_create(uint64_t thing) { CreateAllKidsPanel(reinterpret_cast<CThing*>(thing)); }

}  // extern "C"
