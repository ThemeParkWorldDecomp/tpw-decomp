// Test library for UIInterface::SetGameWindowCursor and PostLevelLoad (unit v5574; libtpw_unit_UIInterface_SetGameWindowCursor.dylib),
// main app: src/game/ui/UIInterface.cpp (also defines gUIPostLoadDate).
// The callees are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ParkTime.h"
#include "UIInterface.h"
#include "World.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gGameWindow;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gOnlinePanel;
CWorld* gGameWorld;

void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);

void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }
void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }
void UI_ClearSelected() { ev("UI_ClearSelected", {}); }
void UI_BankruptAfterLoad() { ev("UI_BankruptAfterLoad", {}); }
void UIMain_EnableAllKeyTables() { ev("UIMain_EnableAllKeyTables", {}); }
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year) { ev("UI_MainPanelDateChange", {day, month, year}); }

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g,
                                    int32_t* h) const {
    int32_t* p[8] = {a, b, c, d, e, f, g, h};
    int64_t v[9];
    v[0] = reinterpret_cast<const uint8_t*>(this) - reinterpret_cast<const uint8_t*>(gGameWorld);
    for (int i = 0; i < 8; i++)
        v[i + 1] = p[i] ? static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p[i]) - reinterpret_cast<const uint8_t*>(&gUIPostLoadDate))
                        : 0xFFFF;
    ev("GetFunnyTime", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]});
    static const char* const keys[7] = {"ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6"};
    for (int i = 0; i < 7; i++)
        if (p[i]) *p[i] = static_cast<int32_t>(ret(keys[i]));
    return TbTimeStamp();
}

extern "C" {

uint64_t tpw_uisgwc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"date", &gUIPostLoadDate}, {"world", &gGameWorld},  {"gw", &gGameWindow},
        {"ow", &gOnlineWindow},     {"op", &gOnlinePanel},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_uisgwc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uisgwc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ParkDate, mYear));
    out[n++] = static_cast<int64_t>(sizeof(ParkDate));
    return n;
}

void tpw_uisgwc_cursor(int16_t cursor) {
    static UIInterface ui;
    ui.SetGameWindowCursor(cursor);
}

void tpw_uisgwc_post() {
    static UIInterface ui;
    ui.PostLevelLoad();
}

}  // extern "C"
