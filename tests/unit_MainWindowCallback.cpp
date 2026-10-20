// Test library for the all-items panel's message functions (unit v5002; libtpw_unit_MainWindowCallback.dylib, tests/test_MainWindowCallback.py), main app:
//   src/MainWindowCallback.cpp   MainWindowCallback, ListTemplateCallback, CreateAllFeaturesPanel, ActualListCallback and the globals they use
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue. Pointers are
// logged as labels (lab(p)); the fakes that answer a pointer take it from the queue (a 32-bit arena address).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "History.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"

using namespace calllog;

namespace {

template <class T>
T* Ptr(int64_t value) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(value));
}

// The 32-bit data word a pointer is kept as (an arena offset; 0 is NULL) and back.
int32_t ToWord(int64_t pointer) { return pointer != 0 ? static_cast<int32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)))) : 0; }
void* FromWord(int32_t word) { return word != 0 ? reinterpret_cast<void*>(tpw::gArenaBase + static_cast<uint32_t>(word)) : nullptr; }

int64_t Lab(const void* pointer) { return static_cast<int64_t>(lab(pointer)); }

// The offset of a member, without offsetof's warning on a class that is not standard layout.
template <class T, class M>
int64_t Off(M T::*member) {
    return reinterpret_cast<char*>(&(reinterpret_cast<T*>(0x1000)->*member)) - reinterpret_cast<char*>(0x1000);
}

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs
void RemoveAllItemsPanel() { ev("RemoveAllItemsPanel", {}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {Lab(thing)}); }
void CreateAllRidesPanel(CThing* thing) { ev("CreateAllRidesPanel", {Lab(thing)}); }
void CreateAllShopsPanel(CThing* thing) { ev("CreateAllShopsPanel", {Lab(thing)}); }
void CreateAllShowsPanel(CThing* thing) { ev("CreateAllShowsPanel", {Lab(thing)}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {static_cast<int64_t>(type), Lab(thing)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int64_t>(message)}); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {Lab(window), int64_t{message}, int64_t{a}, int64_t{b}});
    return static_cast<int32_t>(ret("DefaultPanelMsg"));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {int64_t{id}});
    return Ptr<const char16_t>(ret("GetString", id, 0));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {Lab(window), int64_t{id}, Lab(text)});
}
void UIMain_GotoStatusPanelForThing(void* thing) { ev("GotoStatusPanelForThing", {Lab(thing)}); }
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("ZoomToThing", {int64_t{id.mIndex}}); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {Lab(window), int64_t{message}, int64_t{a}, int64_t{b}});
    return 0;
}
int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("IterateThingList", {static_cast<int64_t>(mask), Lab(reinterpret_cast<const void*>(fn))});
    return 0;
}
void UpdateRideListEntries(InterfaceListControl2* list) { ev("UpdateRideListEntries", {Lab(list)}); }
void UpdateSideshowListEntries(InterfaceListControl2* list) { ev("UpdateSideshowListEntries", {Lab(list)}); }
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {Lab(window), Lab(text), int64_t{a}, int64_t{b}, int64_t{c}, int64_t{d}, int64_t{e}});
}
int32_t FillAllFeaturesListCallback(CThing*) { return 0; }
InterfaceWindow* AllItemsListLCFormat() { return nullptr; }
void AllItemsListLCPostFormat(InterfaceWindow*) {}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Lab(this), int64_t{id}});
    return Ptr<InterfaceWindow>(ret("GetWindow", id, 0));
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {Lab(this), Lab(description), Lab(reinterpret_cast<const void*>(callback))});
    return Ptr<InterfaceWindow>(ret("Build"));
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {Lab(this), Lab(reinterpret_cast<const void*>(callback))});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {Lab(this), int64_t{message}, int64_t{a}, int64_t{b}});
    return static_cast<int32_t>(ret("Default"));
}
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {Lab(this), int64_t{id}, int64_t{ms}});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {Lab(this), int64_t{which}});
    return 0;
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {Lab(this), int64_t{position}});
    *data = ToWord(ret("GetData", position, 0));
    return 1;
}
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {Lab(this), Lab(reinterpret_cast<const void*>(makeRow)), Lab(reinterpret_cast<const void*>(attach))});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {Lab(this), int64_t{column}, static_cast<int64_t>(type)});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {Lab(this), Lab(object)}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {Lab(this)}); }
void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {Lab(this), int64_t{position}, int64_t{item[0].mInt}, int64_t{item[1].mInt}, int64_t{item[2].mInt},
                      int64_t{item[3].mInt}, int64_t{item[4].mInt}, int64_t{columns}, int64_t{update}, int64_t{unused}});
}
int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {Lab(this), Lab(FromWord(data))});
    return static_cast<int32_t>(ret("FindData"));
}
void InterfaceListControl2::SetSort(int32_t column, bool descending) {
    ev("SetSort", {Lab(this), int64_t{column}, int64_t{descending ? 1 : 0}});
}
void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {Lab(this), int64_t{position}}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    char text[9] = {0};
    std::strncpy(text, name, 8);
    text[8] = 0;
    int64_t n = static_cast<int64_t>(std::strlen(text));
    int64_t c[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int64_t i = 0; i < n; ++i) c[i] = static_cast<unsigned char>(text[i]);
    ev("FindDrawObject", {n, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
    return Ptr<InterfaceDrawObject>(ret("FindDrawObject"));
}

int32_t CObject::GetProfitability() const {
    ev("GetProfitability", {Lab(this)});
    return static_cast<int32_t>(ret("GetProfitability"));
}
int32_t CObject::GetTotalRevenue() const {
    ev("GetTotalRevenue", {Lab(this)});
    return static_cast<int32_t>(ret("GetTotalRevenue"));
}
int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {Lab(this)});
    return static_cast<int32_t>(ret("GetCustomerSatisfaction"));
}

template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    // the history is the object's member at the 0x1a4 slot: the event names the object
    const char* object = reinterpret_cast<const char*>(this) - Off(&CObject::mHistory1A4);
    ev("GetSumOfLast", {Lab(object), int64_t{count}});
    sum = static_cast<double>(ret("GetSumOfLast"));
    return 1;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_v5002_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gAllItemsPanel},
        {"ltype", &gAllItemsListType},
        {"list", &gAllItemsList},
        {"sort0", &gAllRidesSort},
        {"sort1", &gAllShopsSort},
        {"sort2", &gAllShowsSort},
        {"sort3", &gAllFeaturesSort},
        {"font", &gAllColumnButtonFont},
        {"script", gAllFeaturesScript},
        {"fn_actual", reinterpret_cast<const void*>(&ActualListCallback)},
        {"fn_lc", reinterpret_cast<const void*>(&AllItemsListLCFormat)},
        {"fn_post", reinterpret_cast<const void*>(&AllItemsListLCPostFormat)},
        {"fn_fill", reinterpret_cast<const void*>(&FillAllFeaturesListCallback)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_v5002_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_v5002_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = Off(&InterfaceListControl2::mItemCount);
    return n;
}

int32_t tpw_v5002_main(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return MainWindowCallback(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
int32_t tpw_v5002_list(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ListTemplateCallback(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
int32_t tpw_v5002_feat(uint64_t thing) {
    CreateAllFeaturesPanel(reinterpret_cast<CThing*>(thing));
    return 0;
}
int32_t tpw_v5002_act(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ActualListCallback(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
