// Test library for the finance graph panel message handler (unit v5073; libtpw_unit_FinanceGraphPanelMsg.dylib, tests/test_FinanceGraphPanelMsg.py), main app:
//   src/FinanceGraphPanelMsg.cpp   FinanceGraphPanelMsg and the globals it defines
//
// The callees outside the unit are recording fakes. Window pointers are opaque numbers (never dereferenced): the fakes log the low 32 bits.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"

using namespace calllog;

namespace {

int64_t Low(const void* p) { return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)))); }

}  // namespace

// ---- CoolRegion / CoolRect (the vtable needs every virtual)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Low(this), id});
    uintptr_t r = 0x60000000u + ((static_cast<uint32_t>(Low(this)) * 31u + static_cast<uint32_t>(id)) & 0x0fffffffu);
    return reinterpret_cast<InterfaceWindow*>(r);
}
int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {Low(this), lab(rect)});
    return 0;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {Low(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {Low(this), state, redraw});
    return 0;
}
void CUIGrapher::SetXRange(int32_t a, int32_t b, int32_t c) { ev("SetXRange", {lab(this), a, b, c}); }
void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) { ev("SetYRange", {lab(this), a, b, c}); }

int32_t GetMaxYValue() { return static_cast<int32_t>(ret("GetMaxYValue")); }
int32_t GetMinYValue() { return static_cast<int32_t>(ret("GetMinYValue")); }
void SetupGraph(CUIGrapher& g, CoolRect& r, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    ev("SetupGraph", {lab(&g), lab(&r), a, b, c, d, e, f});
}
void DrawMeGraph(InterfaceWindow* w, CUIGrapher& g, void (*cb)(CUIGrapher&)) {
    ev("DrawMeGraph", {Low(w), lab(&g), lab(reinterpret_cast<void*>(cb))});
}
void RemoveFinancePanelPanel() { ev("RemoveFinancePanelPanel", {}); }
void KissMyLines(CUIGrapher&) { ev("KissMyLines", {}); }     // only its address is used (DrawMeGraph is a fake)

extern "C" {

uint64_t tpw_fin_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFinanceGraphFlags},
        {"mode", &gFinanceGraphMode},
        {"panel", &gFinanceGraphPanel},
        {"grapher", &gFinanceGrapher},
        {"kiss", reinterpret_cast<const void*>(&KissMyLines)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fin_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fin_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gFinanceGraphFlags));
    out[n++] = static_cast<int64_t>(sizeof(gFinanceGraphMode));
    return n;
}

int32_t tpw_fin_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return FinanceGraphPanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
