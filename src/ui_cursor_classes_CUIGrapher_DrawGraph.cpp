// CUIGrapher::DrawGrid 0x101fdb1c and CUIGrapher::DrawGraph 0x101fddbc (main app).
// The class declares no data members; fields are read by their Mac offsets.
#include <cstdint>
#include <cstring>

#include "FinanceGraphPanelMsg.h"
#include "LbDrawPrimitives.h"
#include "UI_Base.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "platform/msl.h"

char16_t gGrapherNumberFormat[4] = {u'%', u'd', 0, 0};   // 0x1024f70c
int32_t gGrapherFont = 10;                               // 0x102b1558

namespace {

inline int32_t Field(const CUIGrapher* self, int off) {
    int32_t v;
    std::memcpy(&v, reinterpret_cast<const char*>(self) + off, 4);
    return v;
}

inline TbColour ColourAt(const CUIGrapher* self, int off) {
    TbColour c;
    std::memcpy(static_cast<void*>(&c), reinterpret_cast<const char*>(self) + off, 5);
    return c;
}

// divw: the emulator answers the dividend for INT_MIN / -1.
inline int32_t Div(int32_t n, int32_t d) {
    if (d == -1) return n == INT32_MIN ? n : -n;
    return n / d;
}

}  // namespace

void CUIGrapher::DrawGrid() {
    int32_t columns = Field(this, 0x28);
    int32_t rows = Field(this, 0x2c);
    if (columns == 0 || rows == 0) return;

    float xStep = static_cast<float>(Field(this, 4)) / static_cast<float>(rows);       // f29
    float yStep = static_cast<float>(Field(this, 0)) / static_cast<float>(columns);    // f30
    float x = static_cast<float>(Field(this, 8)) + yStep;                              // f28
    float y = 1.0f + (static_cast<float>(Field(this, 0x14)) - xStep);                  // f27

    // Any other column count leaves r29 as the caller's value; 0 here (not tested).
    int32_t skip = 0;
    if (columns == 0x24) skip = 2;
    else if (columns == 0x90) skip = 0xb;
    else if (columns == 0xc) skip = 0;

    int32_t k = 0;
    for (int32_t i = 0; i < Field(this, 0x28); i++) {
        k++;
        if (k > skip) {
            int32_t px = ppc_fctiwz(x);
            k = 0;
            LbDraw_VerticalLine(px, Field(this, 0xc), Field(this, 0x14), ColourAt(this, 0x3a));
            char16_t text[8];
            swprintf(text, 4, gGrapherNumberFormat, i + 1);
            int32_t top = Field(this, 0x14) + 4;
            int32_t left = px;
            if (top < 2) top = 2;
            if (left < 2) left = 2;
            TbRect rect;
            rect.left = left;
            rect.top = top;
            rect.right = left + 0x7d00;
            rect.bottom = top + 0x7d00;
            TbColour label = ColourAt(this, 0x30);
            UI_DrawText(text, rect, label, static_cast<int8_t>(gGrapherFont), 0, 0, 0, nullptr, 1);
        }
        x = x + yStep;
    }
    for (int32_t j = 0; j < Field(this, 0x2c); j++) {
        LbDraw_HorizontalLine(Field(this, 8), ppc_fctiwz(y), Field(this, 0x10), ColourAt(this, 0x3a));
        y = y - xStep;
    }
}

void CUIGrapher::DrawGraph(int32_t* data, int32_t count, int32_t limit, TbColour colour) {
    float step = static_cast<float>(Field(this, 0)) / static_cast<float>(limit);   // f30
    float fx = static_cast<float>(Field(this, 8));                                  // f28
    float fx1 = 0.0f;                                                               // f31
    int32_t yMin = Field(this, 0x1c);
    int32_t range = Field(this, 0x24) - yMin;
    int32_t y0 = 0, y1 = 0;
    int32_t j = 0;
    for (; j < count; j++, data++) {
        if (range != 0) {
            int32_t lo = Field(this, 0x1c);
            int32_t a = Div((data[0] - lo) * Field(this, 4), Field(this, 0x24) - lo);
            int32_t b = Div((data[1] - lo) * Field(this, 4), Field(this, 0x24) - lo);
            y0 = Field(this, 0x14) - a;
            y1 = Field(this, 0x14) - b;
        } else {
            y0 = y1 = Field(this, 0x14);
        }
        fx1 = fx + step;
        float left = static_cast<float>(Field(this, 8));
        float right = static_cast<float>(Field(this, 0x10));
        if (!(fx >= left)) fx = left;
        else if (fx > right) fx = right;
        if (!(fx1 >= left)) fx1 = left;
        else if (fx1 > right) fx1 = right;
        int32_t top = Field(this, 0xc);
        int32_t bottom = Field(this, 0x14);
        if (y0 < top) y0 = top;
        else if (y0 > bottom) y0 = bottom;
        if (y1 < top) y1 = top;
        else if (y1 > bottom) y1 = bottom;
        int32_t ix0 = ppc_fctiwz(fx);
        int32_t ix1 = ppc_fctiwz(fx1);
        LbDraw_Line(ix0, y0, ix1, y1, colour);
        if (limit < 0x90) {
            int32_t xa = ix1 - 1, xb = ix1 + 1;
            LbDraw_Line(xa, y1 - 1, xb, y1 - 1, colour);
            LbDraw_Line(xb, y1 - 1, xb, y1 + 1, colour);
            LbDraw_Line(xb, y1 + 1, xa, y1 + 1, colour);
            LbDraw_Line(xa, y1 + 1, xa, y1 - 1, colour);
        }
        fx = fx + step;
    }
    char16_t text[12];
    swprintf(text, 10, gGrapherNumberFormat, *data);
    int32_t bottom = Field(this, 0x14);
    int32_t left = Field(this, 8);
    int32_t ix = ppc_fctiwz(fx1);
    int32_t top = y1 - 6;
    TbRect out = {0, 0, 0, 0};
    TbRect measure;
    measure.left = left;
    measure.top = top;
    measure.right = ix;
    measure.bottom = bottom;
    UI_MeasureText(text, measure, static_cast<int8_t>(gGrapherFont), 2, 0, 0, &out);
    if (top < 2) top = 2;
    if (left < 2) left = 2;
    int32_t extra = (out.right - out.left) - (ix - left);
    if (extra > 0) ix += extra;
    TbRect rect;
    rect.left = left;
    rect.top = top;
    rect.right = ix;
    rect.bottom = bottom;
    UI_DrawText(text, rect, colour, static_cast<int8_t>(gGrapherFont), 2, 0, 0, nullptr, 1);
}
