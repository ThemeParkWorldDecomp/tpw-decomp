// ui_graph.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "ppc_math.h"
#include "std_deque_errorthingy.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "ui_ime.h"
#include <cstdint>
#include <new>

// Unit v5135: analyser min/max helpers and the errorthingy deque cursor +=.

uint64_t gMslMacAddressHigh = 0;

namespace {
// The jump chain of the listings: 0 -> 0xc, 1 -> 0x24, 2 -> 0x90, anything else 0.
int32_t Lookback(int32_t range) {
    if (range == 1) return 0x24;
    if (range < 1) return range >= 0 ? 0xc : 0;
    return range < 3 ? 0x90 : 0;
}
}  // namespace

// 0x101f5f2c: the entry is truncated to int; no check for -1.
void MaxAnalyserValue(CHistory<float, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetHighestValue(0.0f, Lookback(range));
    float entry;
    history->GetValue(entry, index);
    int32_t truncated = static_cast<int32_t>(entry);
    if (truncated > value) value = truncated;
}

// 0x101f5e3c
void MaxAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetHighestValue(0, Lookback(range));
    int32_t entry;
    history->GetValue(entry, index);
    if (entry > value) value = entry;
}

// 0x101f6030
void MinAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetLowestValue(0, Lookback(range));
    if (index != -1) {
        int32_t entry;
        history->GetValue(entry, index);
        if (entry < value) value = entry;
    }
}

// 0x101f5adc
MslErrorDequeCursor* MslErrorDequeCursorAddErrorthingy(MslErrorDequeCursor* cursor, int32_t count) {
    if (count < 0) {
        errdeque_callee::CursorSub(cursor, -count);
        return cursor;
    }
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mCdeque));
    uint32_t position = table[2] + (cursor->mWord4 + static_cast<uint32_t>(count));
    uint32_t capacity = table[0];
    if (position < capacity)
        cursor->mSlot += static_cast<uint32_t>(count) * 4;
    else
        cursor->mSlot = table[3] + (position - capacity) * 4;
    cursor->mWord4 += static_cast<uint32_t>(count);
    return cursor;
}

// Unit v5202: SetupGraph 0x101f6480, DrawMeGraph 0x101f66c0 and the two GraphLine 0x101f62cc (float) / 0x101f6124 (int).

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

// 0x101f62cc
void GraphLine(CUIGrapher& grapher, CHistory<float, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    GraphLineImpl<float>(grapher, history, colour, step, mode);
}

// 0x101f6124
void GraphLine(CUIGrapher& grapher, CHistory<int32_t, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    GraphLineImpl<int32_t>(grapher, history, colour, step, mode);
}

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

// The destruction of the IME's candidate lists, main app, unit w92:
//   0x101f6bec IME::list_cand_data_t::~list_cand_data_t (100, ui_IME.cpp)
//   0x101f6ecc std::vector<std::basic_string<wchar_t>>::tear_down (124, ui_IME.cpp)
//   0x101f7d18 CUIIME::~CUIIME (116, ui_iterators.cpp)
// The Mac destructors take a deleting flag in r4 and guard against a null object (and, for a subobject, against the member
// offset making it null): on the host the destructor is the first part and `delete p` is the deleting call, as in src/UI_close.cpp.

// 0x101f6ecc. When the vector has a block: each of the `size` strings is destroyed in order (the string library's destructor, the
// flag -1 so that it does not free the element), then the block is freed. The capacity is not read; a vector with no block does nothing
// even when its size is not zero.
template <>
void MslVector<IME::MslWideString>::tear_down() {
    if (mData != nullptr) {
        for (IME::MslWideString* string = mData; string < mData + mSize; ++string) string->~MslWideString();
        ::operator delete(mData);
    }
}

// 0x101f6bec. Lets go of the strings (the first 0x10 bytes hold nothing that needs it).
IME::list_cand_data_t::~list_cand_data_t() {
    mStrings.tear_down();
}
