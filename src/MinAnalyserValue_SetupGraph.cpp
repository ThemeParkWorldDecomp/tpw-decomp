// Unit v5202: SetupGraph 0x101f6480, DrawMeGraph 0x101f66c0 and the two GraphLine 0x101f62cc (float) / 0x101f6124 (int).
#include <cstdint>

#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "ppc_math.h"

int32_t gGraphLineBuffer[160];   // 0x10396480

namespace {

TbColour MakeColour(uint8_t red, uint8_t green, uint8_t blue) {
    TbColour colour;
    colour.mAlpha = 0xff;
    colour.mRed = red;
    colour.mGreen = green;
    colour.mBlue = blue;
    colour.mIndex = LbPalette_FindColour(&_lbGlobalPalette, red, green, blue);
    return colour;
}

// The switch on the mode: 0 -> 12, 1 -> 36, 2 -> 144, anything else 0.
int32_t ModeLimit(int32_t mode) {
    if (mode == 1) return 0x24;
    if (mode < 1) return mode >= 0 ? 0xc : 0;
    return mode < 3 ? 0x90 : 0;
}

inline int32_t ToInt(float value) { return ppc_fctiwz(static_cast<double>(value)); }
inline int32_t ToInt(int32_t value) { return value; }

template <typename T>
void GraphLineImpl(CUIGrapher& grapher, CHistory<T, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    int32_t limit = ModeLimit(mode);
    T value = T();
    int32_t back = history->HowFarBack();
    int32_t count = back < limit ? back : limit;
    if (count < 0) return;
    int32_t* out = gGraphLineBuffer;
    for (int32_t i = count; i >= 0; i--) {
        history->GetValue(value, i * step);
        *out++ = ToInt(value);
    }
    grapher.DrawGraph(gGraphLineBuffer, count, ModeLimit(mode), colour);
}

}  // namespace

// 0x101f6480
void SetupGraph(CUIGrapher& grapher, CoolRect& rect, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    int32_t height = static_cast<int32_t>(gUIRealScreenHeight) * (rect.mBottom - rect.mTop);
    int32_t width = static_cast<int32_t>(gUIRealScreenWidth) * (rect.mRight - rect.mLeft);
    int32_t wpart = (width >> 11) + ((width < 0 && (width & 0x7ff) != 0) ? 1 : 0);
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * height) >> 32);
    int32_t t = hi >> 8;
    int32_t hpart = t + static_cast<int32_t>(static_cast<uint32_t>(t) >> 31);
    grapher.SetDrawArea(4, 4, wpart - 0x14, hpart - 0x14);
    grapher.SetXAxisStyle(MakeColour(0, 0xc8, 0));
    grapher.SetYAxisStyle(MakeColour(0, 0xc8, 0));
    grapher.SetBGStyle(MakeColour(0, 0, 0));
    grapher.SetGridStyle(MakeColour(0, 0x64, 0));
    grapher.SetYRange(a, b, c);
    grapher.SetXRange(d, e, f);
}

// 0x101f62cc
void GraphLine(CUIGrapher& grapher, CHistory<float, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    GraphLineImpl<float>(grapher, history, colour, step, mode);
}

// 0x101f6124
void GraphLine(CUIGrapher& grapher, CHistory<int32_t, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    GraphLineImpl<int32_t>(grapher, history, colour, step, mode);
}

// 0x101f66c0
void DrawMeGraph(InterfaceWindow* window, CUIGrapher& grapher, void (*callback)(CUIGrapher&)) {
    TbRenderArea area;
    int32_t width, height;
    uint8_t* pixels = window->GetClientObjectSurface(&width, &height);
    if (pixels != nullptr) {
        MakeRenderArea(pixels, &area, width, height);
        LbDraw_SetCurrent(&area);
        TbColour black = MakeColour(0, 0, 0);
        TbRect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = width;
        rect.bottom = height;
        LbDraw_RectangleFilled(&rect, black);
        grapher.Clear();
        grapher.DrawGrid();
        grapher.DrawAxes();
        if (callback != nullptr) callback(grapher);
        window->FinishWithClientObjectSurface();
    }
}
