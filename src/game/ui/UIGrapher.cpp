// UIGrapher.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of UIGrapher code, named after it.

#include "AdvisorScoring.h"
#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "GameSys.h"
#include "helpers/ui_2dmapHelpers.h"
#include "LbDrawPrimitives.h"
#include "LbSCBack.h"
#include "lobby.h"
#include "online.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_ezsprite.h"
#include "tb_starfield.h"
#include "TbTList.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "UI_pump.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.

// ---- CUIGrapher
void CUIGrapher::SetXRange(int32_t a, int32_t b, int32_t c) {
    mF28 = c;
    mF18 = a;
    mF20 = b;
}

void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) {
    mF2C = c;
    mF1C = a;
    mF24 = b;
}

void CUIGrapher::SetDrawArea(int32_t a, int32_t b, int32_t c, int32_t d) {
    mF08 = a;
    mF0C = b;
    mF10 = c;
    mF14 = d;
    mF04 = mF14 - mF0C;
    mF00 = mF10 - mF08;
}

// The colour (five bytes) is passed in r4 and the top byte of r5 and copied byte for byte (a word and a byte).
void CUIGrapher::SetXAxisStyle(TbColour colour) { std::memcpy(mXAxis, &colour, 5); }
void CUIGrapher::SetYAxisStyle(TbColour colour) { std::memcpy(mYAxis, &colour, 5); }
void CUIGrapher::SetGridStyle(TbColour colour) { std::memcpy(mGrid, &colour, 5); }
void CUIGrapher::SetBGStyle(TbColour colour) { std::memcpy(mBG, &colour, 5); }

// Unit v51047: std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0 and its constructor 0x101fd6e8,
// CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78 and SetScale
// 0x10231094, the program entry 0x102381a4 and InterfaceKnob::operator- 0x101d038c.

// ---- std::basic_istream<char>::basic_istream(streambuf*) 0x1020a4c0. The object is the Mac's word layout (MacIfstream); r4 is the
// "with virtual base" flag, tested as a signed halfword.
namespace ifstream_char_callee {
void IstreamCtor(MacIfstream* self, int32_t flag, uint32_t streambuf) {
    if (static_cast<int16_t>(flag) != 0) {
        MacOstreamIos* base = reinterpret_cast<MacOstreamIos*>(reinterpret_cast<uint8_t*>(self) + 0xc);
        self->mIos.set(base);
        base->mVtbl = A(gIosBaseVt);
        base->mVtbl = A(gOstreamVt1);
    }
    self->mVtable = A(gIfstreamVtblC);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = A(gIfstreamVtblC) + 0xc;
    ios->mVbaseOffset = static_cast<int32_t>(A(reinterpret_cast<uint8_t*>(self) + 0xc) - A(ios));
    self->mGcount = 0;
    ios = self->mIos.get();
    OstreamIosInit(ios, streambuf);
    ios->mF3c = 0;
    ios->mFill = static_cast<uint8_t>(OstreamWiden(ios, ' '));
}
}  // namespace ifstream_char_callee

// ---- CUIGrapher
void CUIGrapher::Clear() {
    TbRect rect;
    rect.left = mF08;
    rect.top = mF0C;
    rect.right = mF10;
    rect.bottom = mF14;
    TbColour colour;
    colour.mAlpha = mBG[0];
    colour.mRed = mBG[1];
    colour.mGreen = mBG[2];
    colour.mBlue = mBG[3];
    colour.mIndex = mBG[4];
    LbDraw_RectangleFilled(&rect, colour);
}

// Unit v51015: base64enc 0x10202114, Email::Attachment::Attachment 0x10202670, Connection::Connection 0x10201a84,
// Connection::SendCommandLine 0x10201b14, CUIGrapher::DrawAxes 0x101fda4c.

void CUIGrapher::DrawAxes() {
    int32_t y = mF14 + 1;
    if (mF1C < 0) {
        int32_t span = mF24 - mF1C;
        if (span > 0) y = mF14 - (-mF1C * mF04) / span;
    }
    // The bytes are copied into a TbColour without running its constructor (not linked in every library).
    alignas(TbColour) uint8_t xb[sizeof(TbColour)];
    std::memcpy(xb, mXAxis, 5);
    LbDraw_HorizontalLine(mF08 - 1, y, mF10 + 1, *reinterpret_cast<TbColour*>(xb));
    alignas(TbColour) uint8_t yb[sizeof(TbColour)];
    std::memcpy(yb, mYAxis, 5);
    LbDraw_VerticalLine(mF08 - 1, mF0C - 1, mF14 + 1, *reinterpret_cast<TbColour*>(yb));
}

// CUIGrapher::DrawGrid 0x101fdb1c and CUIGrapher::DrawGraph 0x101fddbc (main app).
// The class declares no data members; fields are read by their Mac offsets.

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
