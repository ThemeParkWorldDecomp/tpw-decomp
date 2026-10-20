// Test library for CreateAllShowsPanel (unit v5677; libtpw_unit_CreateAllShowsPanel.dylib), main app:
//   src/LCPostFormat_CreateAllShowsPanel.cpp
// The callees are recording fakes; objects are fixed fake addresses that are never dereferenced.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "InterfaceWindow.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UI_Base.h"
#include "ui_iterators.h"

using namespace calllog;

InterfaceWindow* gAllItemsPanel;
InterfaceListControl2* gAllItemsList;
int32_t gAllItemsListType;
int32_t gAllColumnButtonFont;
int32_t gAllShowsSort;
int16_t gAllShowsScript[2] = {7, 0};

namespace {
const uintptr_t kPanel = 0x7f000100, kGroup = 0x7f000200, kObj400 = 0x7f000300, kList = 0x7f000400, kDraw = 0x7f000500,
                kChild = 0x7f000600;
char16_t gStrings[0x100];
uintptr_t Addr(const void* p) { return reinterpret_cast<uintptr_t>(p); }
}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    uintptr_t self = Addr(this), result = 0;
    if (self == kPanel && id == 0x12c4b8) result = kGroup;
    else if (self == kGroup && id == 400) result = ret("G400") ? kObj400 : 0;
    else if (self == kList && id >= 0x10 && id <= 0x15) result = kChild + id;
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(result);
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gAllShowsScript ? 1 : 0, static_cast<int64_t>(Addr(reinterpret_cast<void*>(callback)))});
    return reinterpret_cast<InterfaceWindow*>(kList);
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == ActualListCallback ? 1 : 0});
}
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), makeRow == AllItemsListLCFormat ? 1 : 0, attach == AllItemsListLCPostFormat ? 1 : 0});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {lab(this), column, type}); }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    int64_t b[8];
    for (int i = 0; i < 8; i++) b[i] = static_cast<uint8_t>(name[i]);
    ev("FindDrawObject", {b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]});
    return reinterpret_cast<InterfaceDrawObject*>(kDraw);
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), static_cast<int64_t>(Addr(object))}); }
void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }
int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("UI_IterateThingList", {mask, fn == FillAllSideshowsListCallback ? 1 : 0});
    return 0;
}
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }
int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), data});
    return static_cast<int32_t>(ret("FindData"));
}
void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), position}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return &gStrings[id & 0xff];
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), static_cast<int64_t>(text - gStrings), a, b, c, d, e});
}
InterfaceWindow* AllItemsListLCFormat() { return nullptr; }
void AllItemsListLCPostFormat(InterfaceWindow*) {}
int32_t ActualListCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t FillAllSideshowsListCallback(CThing*) { return 0; }

extern "C" {

uint64_t tpw_cas_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gAllItemsPanel}, {"font", &gAllColumnButtonFont}, {"sort", &gAllShowsSort},
        {"list", &gAllItemsList},   {"type", &gAllItemsListType},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_cas_run(uint32_t thing) { CreateAllShowsPanel(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }

}  // extern "C"
