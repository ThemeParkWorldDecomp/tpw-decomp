// Test library for the park status panel (unit v5142; libtpw_unit_sinit_ui_parkStatus_cpp.dylib, tests/test_sinit_ui_parkStatus_cpp.py), main app:
//   src/game/ui/ui_parkStatus.cpp   UI_ParkStatusPanelDateChange, __sinit_ui_parkStatus_cpp and their globals
//
// The callees outside the unit are recording fakes: CWorld::GetParkAnalyser, both MaxAnalyserValue, InterfaceWindow::GetWindow,
// CUIGrapher::SetYRange, CUIGrapher_Construct, DrawMeGraph, LbPalette_FindColour.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LbPalette.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

struct TbPalette {
    uint8_t mBytes[1024];
};
TbPalette _lbGlobalPalette;
CWorld* gGameWorld;

namespace {
uint8_t* gTestAnalyser;
uint8_t* gTestChild;
int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p) - gTestAnalyser); }
}  // namespace

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gTestAnalyser);
}

void MaxAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    ev("MaxInt", {Off(history), value, range});
    value = static_cast<int32_t>(ret("max"));
}

void MaxAnalyserValue(CHistory<float, 144>* history, int32_t& value, int32_t range) {
    ev("MaxFloat", {Off(history), value, range});
    value = static_cast<int32_t>(ret("max"));
}

void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) { ev("SetYRange", {lab(this), a, b, c}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("child") ? reinterpret_cast<InterfaceWindow*>(gTestChild) : nullptr;
}

void ParkInfoLines(CUIGrapher&) { ev("ParkInfoLines", {}); }     // only its address is used (DrawMeGraph is a fake)
void DrawMeGraph(InterfaceWindow* window, CUIGrapher& grapher, void (*callback)(CUIGrapher&)) {
    ev("DrawMeGraph", {lab(window), lab(&grapher), lab(reinterpret_cast<const void*>(callback))});
}

void CUIGrapher_Construct(CUIGrapher* self) { ev("CUIGrapherCtor", {lab(self)}); }

uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    ev("FindColour", {lab(palette), red, green, blue});
    return static_cast<uint8_t>(ret("colour"));
}

extern "C" {

uint64_t tpw_parkstatus_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"colours", gParkStatusColours},
        {"grapher", &gParkStatusGrapher},
        {"range", &gParkStatusRange},
        {"panel", &gParkStatusPanel},
        {"flags", &gParkStatusFlags},
        {"world", &gGameWorld},
        {"lines", reinterpret_cast<const void*>(&ParkInfoLines)},
        {"palette", &_lbGlobalPalette},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_parkstatus_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_parkstatus_set(uint64_t analyser, uint64_t child) {
    gTestAnalyser = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(analyser));
    gTestChild = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(child));
}

int32_t tpw_parkstatus_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gParkStatusColours));
    return n;
}

void tpw_parkstatus_sinit() { __sinit_ui_parkStatus_cpp(); }
void tpw_parkstatus_date(int32_t month) { UI_ParkStatusPanelDateChange(month); }

}  // extern "C"
