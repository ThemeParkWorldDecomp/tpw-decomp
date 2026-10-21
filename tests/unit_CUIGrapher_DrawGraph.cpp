// Test library for CUIGrapher::DrawGrid / DrawGraph (unit v5160; libtpw_unit_CUIGrapher_DrawGraph.dylib, tests/test_CUIGrapher_DrawGraph.py):
//   src/game/ui/UIGrapher.cpp
//
// The callees outside the unit (LbDraw_Line, LbDraw_VerticalLine, LbDraw_HorizontalLine, swprintf, UI_DrawText, UI_MeasureText) are fakes
// that record what the real ones read. The two globals of the unit are defined by the src file itself.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "LbDrawPrimitives.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "platform/msl.h"

using namespace calllog;

// Defined by another file of the program; the unit overwrites every byte after constructing.
TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}

namespace {

int64_t ColWord(const TbColour& c) {
    return (static_cast<int64_t>(c.mAlpha) << 24) | (c.mRed << 16) | (c.mGreen << 8) | c.mBlue;
}

int64_t TextHash(const char16_t* t) {
    uint32_t n = 0, h = 0;
    while (t[n] != 0 && n < 32) {
        h = h * 31u + t[n];
        n++;
    }
    return static_cast<int64_t>(h * 31u + n);
}

}  // namespace

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t value = va_arg(ap, int32_t);
    va_end(ap);
    ev("swprintf", {static_cast<int64_t>(size), (static_cast<int64_t>(format[0]) << 16) | format[1], value});
    char tmp[16];
    int n = std::snprintf(tmp, sizeof tmp, "%d", value);
    uint32_t i = 0;
    for (; i < static_cast<uint32_t>(n) && i + 1 < size; i++) buffer[i] = static_cast<char16_t>(tmp[i]);
    buffer[i] = 0;
    return n;
}

void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour) {
    ev("Line", {x0, y0, x1, y1, ColWord(colour), colour.mIndex});
}

void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    ev("VLine", {x, y, length, ColWord(colour), colour.mIndex});
}

void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    ev("HLine", {x, y, length, ColWord(colour), colour.mIndex});
}

void UI_DrawText(const char16_t* text, const TbRect& rect, const TbColour& colour, int8_t font, int8_t horizontal, int8_t vertical,
                 int32_t wrap, TbRect* clip, int32_t flag) {
    int64_t packed = (static_cast<int64_t>(horizontal & 255) << 24) | ((vertical & 255) << 16) | ((wrap & 255) << 8) |
                     (clip != nullptr ? 2 : 0) | (flag & 1);
    ev("DrawText", {TextHash(text), rect.left, rect.top, rect.right, rect.bottom, ColWord(colour), colour.mIndex, font, packed});
}

void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* out) {
    ev("Measure", {TextHash(text), rect.left, rect.top, rect.right, rect.bottom, font, horizontal, vertical, wrap, out != nullptr});
    out->left = static_cast<int32_t>(ret("ml"));
    out->top = 0;
    out->right = static_cast<int32_t>(ret("mr"));
    out->bottom = 0;
}

extern "C" {

uint64_t tpw_gr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", &gGrapherFont},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gGrapherFont));
    return n;
}

void tpw_gr_grid(uint64_t self) { reinterpret_cast<CUIGrapher*>(static_cast<uintptr_t>(self))->DrawGrid(); }

void tpw_gr_graph(uint64_t self, uint64_t data, int32_t count, int32_t limit, uint32_t cword, uint32_t cindex) {
    TbColour c;
    c.mAlpha = cword >> 24;
    c.mRed = cword >> 16;
    c.mGreen = cword >> 8;
    c.mBlue = cword;
    c.mIndex = static_cast<uint8_t>(cindex);
    reinterpret_cast<CUIGrapher*>(static_cast<uintptr_t>(self))
        ->DrawGraph(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(data)), count, limit, c);
}

}  // extern "C"
