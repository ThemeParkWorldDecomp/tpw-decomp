// ltms TbFontRender.cpp, the 4-bit font render method's core (unit w45a; ltms symbols shared 0x10000bf0..0x10001f64): the
// method's constructor and destructors, DrawChar (decodes one glyph and hands it to the writer for the render area's
// pixel format), CalculateSourceRect, GetRotate and the decode buffer FourBit::FontData. The nybble and bit iterators
// (include/TbFontBits.h) are inline. The writers (DrawCharTo*) and the four method objects are unit w45b's.
#include <cstring>
#include <new>
#include <vector>

#include "LbDraw.h"
#include "TbFontBits.h"
#include "TbFontRender.h"
#include "TbIRLE4bitFont.h"

// ---- the destructors

// 0x10000c38: the base class's destructor (the original also deletes `this` when its hidden flag is positive: the
// compiler's deleting form).
TbFontRenderMethod::~TbFontRenderMethod() {}

// 0x10000cb4
Tb4BitFontRenderMethod::~Tb4BitFontRenderMethod() {}

// ---- the decode buffer

bool FourBit::FontData::sMade = false;

namespace {

// The instance FontData::I answers (Mac: the words at 0x100116fc and 0x10011700). Its destructor runs at exit.
FourBit::FontData gFontData = {0, nullptr};

}  // namespace

// 0x10001d30
FourBit::FontData::~FontData() { delete[] mBlock; }

// 0x100018d8: the first use sets the size word to 0x800 and makes a block of that size (the original registers the
// destructor with __register_global_object here and sets the flag byte at 0x10011704).
FourBit::FontData& FourBit::FontData::I() {
    if (!sMade) {
        gFontData.mSize = 0x800;
        gFontData.mBlock = new uint8_t[0x800];
        sMade = true;
    }
    return gFontData;
}

// ---- the glyph rectangle

// 0x10001dbc. The rectangle moved to `point` is cut to the clip rectangle (a copy of it is taken first), and the source
// starts where the glyph does plus the amount the point lies left of or above the clip. The area (third argument) is
// not read.
TbRect Tb4BitFontRenderMethod::CalculateSourceRect(const TbRect& rect, const TbPoint& point, const TbRenderArea& area) {
    const int32_t width = rect.right - rect.left;
    const int32_t height = rect.bottom - rect.top;
    const TbRect moved = {point.x, point.y, point.x + width, point.y + height};
    const TbRect* const clipRect = LbDraw_GetClipRect();
    const TbRect clip = {clipRect->left, clipRect->top, clipRect->right, clipRect->bottom};
    const TbRect inside = moved.Intersection(clip);
    int32_t left = rect.left;
    int32_t top = rect.top;
    if (point.x < clip.left) left += clip.left - point.x;
    if (point.y < clip.top) top += clip.top - point.y;
    return {left, top, left + (inside.right - inside.left), top + (inside.bottom - inside.top)};
}

// 0x10001f64: the count starts at 32 and the mask 0x80000000; both step down until the mask has a bit in common with the
// argument. For 0 the count also steps once more past 0 (to 0xffffffff) before the loop gives up.
int32_t Tb4BitFontRenderMethod::GetRotate(uint32_t mask) {
    uint32_t count = 32;
    uint32_t bit = 0x80000000u;
    while ((mask & bit) == 0) {
        const bool atZero = count == 0;
        --count;
        if (atZero) break;
        bit >>= 1;
    }
    if (count >= 8) return static_cast<int32_t>(count - 8);
    return static_cast<int32_t>(count + 0x18);
}

// ---- DrawChar

namespace {

// A word of a character header (host order: the font loader has swapped the file's words).
uint16_t HalfAt(const uint8_t* header, int offset) {
    uint16_t value;
    std::memcpy(&value, header + offset, sizeof(value));
    return value;
}
uint32_t WordAt(const uint8_t* header, int offset) {
    uint32_t value;
    std::memcpy(&value, header + offset, sizeof(value));
    return value;
}

// The render area's mask word `index` (the four channel masks are the first four words of the area).
uint32_t MaskOf(const TbRenderArea& area, int index) {
    uint32_t value;
    std::memcpy(&value, area.mF00 + 4 * index, sizeof(value));
    return value;
}

// The original's test of the area for one pixel format: four masks, the depth and the palettised flag (the last is 0 by
// then, DrawChar has already refused a palettised area).
bool IsFormat(const TbRenderArea& area, uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha, int32_t depth) {
    return MaskOf(area, 0) == red && MaskOf(area, 1) == green && MaskOf(area, 2) == blue && MaskOf(area, 3) == alpha &&
           static_cast<uint32_t>(area.mDepth) == static_cast<uint32_t>(depth) && area.mPalettised == 0;
}

// The decode buffer, made (the 0x800 bytes) and grown to hold `dataSize` bytes; the compare is signed. Answers the block.
uint8_t* BufferFor(uint32_t dataSize) {
    FourBit::FontData& buffer = FourBit::FontData::I();
    if (static_cast<int32_t>(buffer.mSize) < static_cast<int32_t>(dataSize)) {
        buffer.mSize = dataSize;
        delete[] buffer.mBlock;
        buffer.mBlock = new uint8_t[buffer.mSize];
    }
    return buffer.mBlock;
}


}  // namespace

// 0x10000d40. The font comes from the parameters with a reference taken (GetFont); every path after that lets it go
// through its virtual Release (slot 8). The original reads _lbpCurrentRA before GetFont and dereferences it only after;
// a NULL font never gets that far, and the original then calls Release through a NULL object, which is not reproduced.
TbError Tb4BitFontRenderMethod::DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn, TbFontRenderParams& params) {
    TbRenderArea* const area = _lbpCurrentRA;
    TbIRLE4bitFont* const font = static_cast<TbIRLE4bitFont*>(params.GetFont());
    if (font == nullptr) return TbError(-1);
    if (area->mPalettised != 0) {
        font->Release();
        return TbError(-1);
    }
    const uint8_t* const header = font->GetCharHeader(glyph);
    const uint8_t* const data = font->GetCharData(glyph);
    if (data == nullptr) {
        font->Release();
        return TbError(-1);
    }
    const uint32_t flags = WordAt(header, 0xc);
    const uint32_t dataSize = WordAt(header, 8);
    if ((flags & 1) != 0) {
        // Nybble run-length data: a nonzero nybble is one pixel; 0, n, v is n pixels of v; 0, 0 ends the glyph.
        uint8_t* const block = BufferFor(dataSize);
        FourBit::NybbleIterator in(const_cast<uint8_t*>(data), false);
        FourBit::NybbleWriter out(block, false);
        bool done = false;
        while (!done) {
            const uint8_t nybble = *in++;
            if (nybble == 0) {
                const uint8_t count = *in++;
                if (count == 0) {
                    done = true;
                } else {
                    const uint8_t value = *in++;
                    for (uint32_t left = count; left != 0; --left) out.Write(value);
                }
            } else {
                out.Write(nybble);
            }
        }
        out.Nybbles();
    }
    if ((flags & 2) != 0) {
        // One-bit data: twice the data size in pixels (not eight times: the original's count), each bit 0 or 0xf.
        uint8_t* const block = BufferFor(dataSize);
        OneBit::BitIterator in(const_cast<uint8_t*>(data), 0);
        FourBit::NybbleWriter out(block, false);
        for (uint32_t left = dataSize << 1; left != 0; --left) {
            const OneBit::BitIterator::BitReference bit = *in++;
            out.Write(static_cast<uint8_t>(bit) != 0 ? 0xf : 0);
        }
    }
    uint16_t width = HalfAt(header, 0x10);
    uint16_t height = HalfAt(header, 0x12);
    const uint8_t* pixels = data;
    if ((flags & 3) != 0) pixels = FourBit::FontData::I().mBlock;
    int32_t offsetX = static_cast<int8_t>(header[0x14]);
    int32_t offsetY = static_cast<int8_t>(header[0x15]);
    const TbRect glyphRect = {0, 0, width, height};
    TbPoint point = at;
    if (HalfAt(header, 0) == 0x20) {
        // A space draws nothing: the empty rectangle at the point as it was given.
        drawn = {point.x, point.y, point.x, point.y};
        font->Release();
        return TbError(0);
    }
    point.x += offsetX;
    point.y += offsetY;
    const TbRect source = CalculateSourceRect(glyphRect, point, *area);
    if (source.left == source.right || source.top == source.bottom) {
        font->Release();
        return TbError(-1);
    }
    const TbRect* const clip = LbDraw_GetClipRect();
    if (point.x < clip->left) point.x = clip->left;
    if (point.y < clip->top) point.y = clip->top;
    drawn = {point.x, point.y, point.x + (source.right - source.left), point.y + (source.bottom - source.top)};
    C4BitFontRenderMethodParameters record;
    record.mPoint = &point;
    record.mData = pixels;
    record.mBytesPerRow = (width + 1) >> 1;
    record.mSource = &source;
    record.mParams = &params;
    record.mWidth = static_cast<uint8_t>(width);

    // The writer for the area's format. The original tests every format in turn (not as an else chain) and calls the
    // generic writer only when none matched; the formats exclude each other, so one writer runs.
    int32_t result = 0;
    bool written = false;
    if (IsFormat(*area, 0x7c00, 0x3e0, 0x1f, 0x8000, 16) || IsFormat(*area, 0x7c00, 0x3e0, 0x1f, 0, 16)) {
        result = DrawCharTo555(*area, record);
        written = true;
    }
    if (IsFormat(*area, 0xf800, 0x7e0, 0x1f, 0, 16)) {
        result = DrawCharTo565(*area, record);
        written = true;
    }
    if (IsFormat(*area, 0xf000, 0xf00, 0xf0, 0xf, 16)) {
        result = DrawCharTo4444(*area, record);
        written = true;
    }
    if (IsFormat(*area, 0xff0000, 0xff00, 0xff, 0xff000000u, 32) || IsFormat(*area, 0xff0000, 0xff00, 0xff, 0, 32)) {
        result = DrawCharTo8888(*area, record);
        written = true;
    }
    if (IsFormat(*area, 0xff00, 0xff0000, 0xff000000u, 0xff, 32) || IsFormat(*area, 0xff00, 0xff0000, 0xff000000u, 0, 32)) {
        result = DrawCharToBGRA(*area, record);
        written = true;
    }
    if (!written) result = DrawCharToAny(*area, record);
    font->Release();
    return TbError(result);
}
