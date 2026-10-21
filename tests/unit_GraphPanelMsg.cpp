// Test library for the park status graph panel's message handler (unit v5894; libtpw_unit_GraphPanelMsg.dylib, tests/test_GraphPanelMsg.py), main app:
//   src/game/ui/ui_parkStatus.cpp   GraphPanelMsg
//
// The callees outside the unit (GetParkAnalyser, MaxAnalyserValue x2, SetupGraph, SetXRange, SetYRange, DrawMeGraph, GetClientRect,
// CallDefaultWindowHandler) are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "analyser.h"
#include "History.h"
#include "UI_Base.h"

using namespace calllog;

// ---- globals of other files
CUIGrapher gParkStatusGrapher;
int32_t gParkStatusRange;
uint32_t gParkStatusFlags;
CWorld* gGameWorld;

namespace {
const uintptr_t kAnalyserBase = 0x30000000u;
}

// ---- the fakes
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {1});
    return reinterpret_cast<CParkAnalyser*>(kAnalyserBase);
}

void MaxAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    ev("MaxI", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(history) - kAnalyserBase), value, range});
    value = static_cast<int32_t>(ret("Max"));
}

void MaxAnalyserValue(CHistory<float, 144>* history, int32_t& value, int32_t range) {
    ev("MaxF", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(history) - kAnalyserBase), value, range});
    value = static_cast<int32_t>(ret("Max"));
}

void SetupGraph(CUIGrapher& grapher, CoolRect& rect, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    ev("SetupGraph", {lab(&grapher), rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, a, b, c, d, e, f});
}

void CUIGrapher::SetXRange(int32_t a, int32_t b, int32_t c) { ev("SetXRange", {lab(this), a, b, c}); }
void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) { ev("SetYRange", {lab(this), a, b, c}); }

void ParkInfoLines(CUIGrapher&) { ev("ParkInfoLines", {}); }     // only its address is used (DrawMeGraph is a fake)
void DrawMeGraph(InterfaceWindow* window, CUIGrapher& grapher, void (*callback)(CUIGrapher&)) {
    ev("DrawMeGraph", {lab(window), lab(&grapher), lab(reinterpret_cast<void*>(callback))});
}

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this)});
    rect->mLeft = static_cast<int16_t>(ret("RL"));
    rect->mTop = static_cast<int16_t>(ret("RT"));
    rect->mRight = static_cast<int16_t>(ret("RR"));
    rect->mBottom = static_cast<int16_t>(ret("RB"));
    return 0;
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

extern "C" {

uint64_t tpw_gpm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"grapher", &gParkStatusGrapher}, {"range", &gParkStatusRange}, {"flags", &gParkStatusFlags},
        {"lines", reinterpret_cast<const void*>(&ParkInfoLines)},        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gpm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gpm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gParkStatusFlags));
    return n;
}

uint32_t tpw_gpm_call(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(GraphPanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}

}  // extern "C"
