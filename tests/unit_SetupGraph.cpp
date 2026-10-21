// Test library for the graph helpers (unit v5202; libtpw_unit_SetupGraph.dylib, tests/test_SetupGraph.py), main app:
//   src/game/ui/ui_graph.cpp   SetupGraph, DrawMeGraph, GraphLine (float and int)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the
// globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"

using namespace calllog;

int16_t gUIRealScreenHeight;
int16_t gUIRealScreenWidth;
struct TbPalette {
    uint8_t mEntries[0x400];
};
TbPalette _lbGlobalPalette;
static int32_t gGetCount;   // the number of GetValue calls of the case (reset by the graphline wrappers)

namespace {

uint32_t Pack(TbColour c) {
    return (uint32_t(c.mAlpha) << 24) | (uint32_t(c.mRed) << 16) | (uint32_t(c.mGreen) << 8) | c.mBlue;
}

float BitsF(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

void Style(const char* name, CUIGrapher* self, TbColour c) { ev(name, {lab(self), int64_t(Pack(c)), int64_t(c.mIndex)}); }

void Callback(CUIGrapher& g) { ev("Callback", {lab(&g)}); }

}  // namespace

TbColour::TbColour() {}
TbRenderArea::TbRenderArea() { ev("RenderAreaCtor", {}); }

void CUIGrapher::SetXRange(int32_t a, int32_t b, int32_t c) { ev("SetXRange", {lab(this), a, b, c}); }
void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) { ev("SetYRange", {lab(this), a, b, c}); }
void CUIGrapher::SetDrawArea(int32_t a, int32_t b, int32_t c, int32_t d) { ev("SetDrawArea", {lab(this), a, b, c, d}); }
void CUIGrapher::SetXAxisStyle(TbColour c) { Style("SetXAxisStyle", this, c); }
void CUIGrapher::SetYAxisStyle(TbColour c) { Style("SetYAxisStyle", this, c); }
void CUIGrapher::SetGridStyle(TbColour c) { Style("SetGridStyle", this, c); }
void CUIGrapher::SetBGStyle(TbColour c) { Style("SetBGStyle", this, c); }
void CUIGrapher::Clear() { ev("Clear", {lab(this)}); }
void CUIGrapher::DrawAxes() { ev("DrawAxes", {lab(this)}); }
void CUIGrapher::DrawGrid() { ev("DrawGrid", {lab(this)}); }
void CUIGrapher::DrawGraph(int32_t* data, int32_t count, int32_t limit, TbColour c) {
    ev("DrawGraph", {lab(this), data == gGraphLineBuffer ? 1 : 0, count, limit, int64_t(Pack(c)), int64_t(c.mIndex)});
}

uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    ev("FindColour", {lab(palette), red, green, blue});
    return static_cast<uint8_t>(ret("FindColour"));
}

void MakeRenderArea(uint8_t* pixels, TbRenderArea*, int32_t width, int32_t height) {
    ev("MakeRenderArea", {int64_t(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pixels))), width, height});
}
void LbDraw_SetCurrent(TbRenderArea*) { ev("SetCurrent", {}); }
void LbDraw_RectangleFilled(const TbRect* r, TbColour c) {
    ev("RectFilled", {r->left, r->top, r->right, r->bottom, int64_t(Pack(c)), int64_t(c.mIndex)});
}

uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* width, int32_t* height) {
    ev("GetSurface", {lab(this)});
    *width = static_cast<int32_t>(ret("w"));
    *height = static_cast<int32_t>(ret("h"));
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("surface"))));
}
void InterfaceWindow::FinishWithClientObjectSurface() { ev("Finish", {lab(this)}); }

template <>
int32_t CHistory<float, 144>::HowFarBack() const {
    ev("HowFarBack", {lab(this)});
    return static_cast<int32_t>(ret("HowFarBack"));
}
template <>
int32_t CHistory<int32_t, 144>::HowFarBack() const {
    ev("HowFarBack", {lab(this)});
    return static_cast<int32_t>(ret("HowFarBack"));
}
template <>
bool CHistory<float, 144>::GetValue(float& out, int32_t back) const {
    ev("GetValue", {lab(this), back});
    int32_t group = gGetCount++ / 32;
    bool ok = (ret("GetOk", group, 0) & 0xff) != 0;
    uint32_t v = static_cast<uint32_t>(ret("GetValue", group, 0));
    if (ok) out = BitsF(v);
    return ok;
}
template <>
bool CHistory<int32_t, 144>::GetValue(int32_t& out, int32_t back) const {
    ev("GetValue", {lab(this), back});
    int32_t group = gGetCount++ / 32;
    bool ok = (ret("GetOk", group, 0) & 0xff) != 0;
    uint32_t v = static_cast<uint32_t>(ret("GetValue", group, 0));
    if (ok) out = static_cast<int32_t>(v);
    return ok;
}

extern "C" {

uint64_t tpw_v5202_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"h", &gUIRealScreenHeight}, {"w", &gUIRealScreenWidth}, {"pal", &_lbGlobalPalette}, {"buf", gGraphLineBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5202_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5202_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbColour));
    out[n++] = static_cast<int64_t>(offsetof(TbColour, mIndex));
    return n;
}

void tpw_v5202_setup(uint64_t grapher, int32_t l, int32_t t, int32_t r, int32_t b, int32_t a, int32_t bb, int32_t c, int32_t d,
                     int32_t e, int32_t f) {
    alignas(16) unsigned char storage[sizeof(CoolRect) + 16] = {};
    CoolRect* rect = reinterpret_cast<CoolRect*>(storage);
    rect->mLeft = static_cast<int16_t>(l);
    rect->mTop = static_cast<int16_t>(t);
    rect->mRight = static_cast<int16_t>(r);
    rect->mBottom = static_cast<int16_t>(b);
    SetupGraph(*reinterpret_cast<CUIGrapher*>(grapher), *rect, a, bb, c, d, e, f);
}

void tpw_v5202_graphline_f(uint64_t grapher, uint64_t history, uint64_t colour, int32_t step, int32_t mode) {
    gGetCount = 0;
    GraphLine(*reinterpret_cast<CUIGrapher*>(grapher), reinterpret_cast<CHistory<float, 144>*>(history),
              *reinterpret_cast<TbColour*>(colour), step, mode);
}
void tpw_v5202_graphline_i(uint64_t grapher, uint64_t history, uint64_t colour, int32_t step, int32_t mode) {
    gGetCount = 0;
    GraphLine(*reinterpret_cast<CUIGrapher*>(grapher), reinterpret_cast<CHistory<int32_t, 144>*>(history),
              *reinterpret_cast<TbColour*>(colour), step, mode);
}
void tpw_v5202_drawme(uint64_t window, uint64_t grapher, int32_t hasCallback) {
    DrawMeGraph(reinterpret_cast<InterfaceWindow*>(window), *reinterpret_cast<CUIGrapher*>(grapher), hasCallback ? Callback : nullptr);
}

}  // extern "C"
