// Test library for two status panel window handlers (unit v5753; libtpw_unit_ViewWindowCallback.dylib, tests/test_ViewWindowCallback.py), main app:
//   src/game/ui/ui_StatusPanel.cpp   ViewWindowCallback, StatusPanelTitleMsg
//
// The callees outside the unit are recording fakes. The "windows" are raw blocks: word 0 is a marker the fakes log, word 1 (+4) the
// extra parameter (an arena offset on the host). The panel block holds its marker at +0x24 and the table pointer at +0x20.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "InterfaceWindow.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "analyser.h"

using namespace calllog;

CThing* gStaffRoomPanelThing;

namespace {

uint32_t Word(const void* base, size_t offset) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(base) + offset); }

void Fn34(void* panel) { ev("slot", {0x34, static_cast<int64_t>(Word(panel, 0x24))}); }
void Fn40(void* panel) { ev("slot", {0x40, static_cast<int64_t>(Word(panel, 0x24))}); }

}  // namespace

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {static_cast<int64_t>(Word(this, 0)), index});
    return reinterpret_cast<void*>(tpw::gArenaBase + Word(this, 4));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {static_cast<int64_t>(Word(this, 0)), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {static_cast<int64_t>(Word(this, 0)), red, green, blue, alpha});
}

void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {static_cast<int64_t>(Word(window, 0))}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {mThingId});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = mThingId;
    return id;
}

void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("UIMain_ZoomToThing", {id.mIndex}); }

extern "C" {

uint64_t tpw_vwc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"fn34", reinterpret_cast<const void*>(&Fn34)},
        {"fn40", reinterpret_cast<const void*>(&Fn40)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_vwc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_vwc_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_vwc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mThingId));
    return n;
}

uint32_t tpw_vwc_view(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(ViewWindowCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}

uint32_t tpw_vwc_title(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(StatusPanelTitleMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}

}  // extern "C"
