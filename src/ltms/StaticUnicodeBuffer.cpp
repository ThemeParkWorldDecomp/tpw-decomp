// StaticUnicodeBuffer.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of StaticUnicodeBuffer code, named after it.

#include "CCharacterHandler.h"
#include "CStaticBuffer.h"
#include "LbDraw.h"
#include "TbFontBits.h"
#include "TbFontRender.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include <cstring>
#include <new>
#include <vector>

// 0x10000024
CCharacterHandler::CCharacterHandler() {}

// 0x10000060
CCharacterHandler::~CCharacterHandler() {}

// CStaticMBBuffer (ltms): the multibyte output buffer of TbITable::FormatString. The destructor is the key function (src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp, unit v51128), so the vtable lands there.

// 0x100000dc. A size of 0xffffffff means "up to the terminating zero". Stops quietly when the buffer is full (the flag is set when the
// cursor reaches the end, and is tested before every byte).
void CStaticMBBuffer::AppendMB(const unsigned char* data, uint32_t size) {
    if (!(mCursor < mEnd)) return;
    if (size == 0xffffffffu) {
        while (*data != 0) {
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *data++;
        }
        return;
    }
    for (;;) {
        const bool done = (size == 0);
        size--;
        if (done) return;
        if (!mOverflow && mCursor == mEnd) mOverflow = true;
        if (mOverflow) return;
        *mCursor++ = *data++;
    }
}

// 0x100001e8. Low byte, then the high byte when it is not zero (the second byte is written without a room test: BUG kept).
void CStaticMBBuffer::AppendMB(uint16_t value) {
    if (!mOverflow && mCursor == mEnd) mOverflow = true;
    if (mOverflow) return;
    *mCursor++ = static_cast<unsigned char>(value);
    const unsigned char high = static_cast<unsigned char>(value >> 8);
    if (high == 0) return;
    *mCursor++ = high;
}

// 0x10000284. No converter: nothing. Each character goes through the virtual AppendMB(uint16_t) (vtable +0x14).
void CStaticMBBuffer::AppendUnicode(const char16_t* text, uint32_t size) {
    if (mConverter == nullptr) return;
    if (size == 0xffffffffu) {
        while (*text != 0) {
            const char16_t c = *text++;
            AppendMB(mConverter->GetMBValue(c));
        }
        return;
    }
    for (;;) {
        const bool done = (size == 0);
        size--;
        if (done) return;
        const char16_t c = *text++;
        AppendMB(mConverter->GetMBValue(c));
    }
}

// 0x10000394
void CStaticMBBuffer::AppendUnicode(char16_t c) {
    if (mConverter == nullptr) return;
    AppendMB(mConverter->GetMBValue(c));
}

// 0x10000420. Terminates the text (at the cursor, or at the last byte when the buffer is full), forgets the cursor, lets go of the converter.
void CStaticMBBuffer::Close() {
    unsigned char* cursor = mCursor;
    if (cursor != nullptr) {
        if (!mOverflow && cursor == mEnd) mOverflow = true;
        if (!mOverflow) {
            *mCursor = 0;
            mCursor = nullptr;
        } else {
            mCursor[-1] = 0;
            mCursor = nullptr;
        }
    }
    TbIUnicodeToMB* converter = mConverter;
    if (converter != nullptr) {
        mConverter = nullptr;
        converter->Release();
    }
}

// CStaticUnicodeBuffer (ltms library, 0x100004f0..0x100009d8; unit w40): the output buffer of the string table. TbITable::FormatString
// makes one over the caller's array (cursor at the start, end past the last character, the table's multibyte to Unicode converter,
// the overflow flag clear), the table's formatting bytecode appends to it through the virtual AppendMB and AppendUnicode, and Close
// terminates the text. The class is declared in include/CStaticBuffer.h.

namespace {

// The converter's Unicode character for the multibyte character at `cursor`, which moves past it: the table entry for
// TbIMBInfo::GetIndex's index (taken as 16 bits), or 0 when the index is not below the converter's count.
char16_t ConvertOne(const TbIMBToUnicode* converter, const unsigned char*& cursor) {
    const uint32_t index = converter->GetIndex(cursor) & 0xffff;
    return index < converter->mCount ? converter->mTable[index] : 0;
}

}  // namespace

// 0x100004f0. A size of 0xffffffff means "up to the terminating zero". With any other the loop ends when the cursor reaches
// data + size EXACTLY: a two-byte character that straddles the end moves the cursor past it and the loop runs on (BUG kept).
void CStaticUnicodeBuffer::AppendMB(const unsigned char* data, uint32_t size) {
    if (mConverter == nullptr) return;
    const unsigned char* cursor = data;
    if (size == 0xffffffffu) {
        while (*cursor != 0) AppendUnicode(ConvertOne(mConverter, cursor));
    } else {
        const unsigned char* end = data + size;
        while (cursor != end) AppendUnicode(ConvertOne(mConverter, cursor));
    }
}

// 0x100006bc. The Mac keeps the halfword in memory big-endian, so GetIndex reads its high byte first.
void CStaticUnicodeBuffer::AppendMB(uint16_t value) {
    TbIMBToUnicode* converter = mConverter;
    if (converter == nullptr) return;
    const unsigned char bytes[2] = {static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value)};
    const unsigned char* cursor = bytes;
    AppendUnicode(ConvertOne(converter, cursor));
}

// ltms: multibyte character sets and the multibyte to Unicode converter (ltms shared,
// 0x1000063c, 0x10002f3c, 0x10003204..0x10003338). Their loaders are in
// TbResourceFactories.cpp.

uint32_t TbIMBInfo::GetIndex(const unsigned char*& text) const {
    uint32_t index = static_cast<uint32_t>(*text++) - 1;
    if (mFirstLead <= (index & 0xffff)) {
        index = ((index & 0xffff) * 0xff + static_cast<uint32_t>(mFirstLead) * static_cast<uint32_t>(-0xfe) +
                 *text++) - 1;
    }
    return index;
}

// 0x10000780. The character is tested against the end of the buffer before each is written, so the buffer can be filled exactly
// and the next one, not the last, raises the overflow flag; once it is raised the rest of the text is dropped.
void CStaticUnicodeBuffer::AppendUnicode(const char16_t* text, uint32_t count) {
    if (count == 0xffffffffu) {
        while (*text != 0) {
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *text++;
        }
    } else {
        while (count != 0) {
            --count;
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *text++;
        }
    }
}

// 0x10000884
void CStaticUnicodeBuffer::AppendUnicode(char16_t value) {
    if (!mOverflow && mCursor == mEnd) mOverflow = true;
    if (mOverflow) return;
    *mCursor++ = value;
}
// 0x10000904. Close drops the converter itself, so the destructor never finds one.
void CStaticUnicodeBuffer::Close() {
    if (mCursor != nullptr) {
        if (!mOverflow && mCursor == mEnd) mOverflow = true;
        if (!mOverflow) {
            *mCursor = 0;
        } else {
            mCursor[-1] = 0;
        }
        mCursor = nullptr;
    }
    TbIMBToUnicode* converter = mConverter;
    if (converter != nullptr) {
        mConverter = nullptr;
        converter->Release();
    }
}

// 0x100009d8. The Mac's also resets the vtable to the class's own on entry and to the base class's on exit, and frees the object
// when its hidden flag is positive; the compiler does both here.
CStaticUnicodeBuffer::~CStaticUnicodeBuffer() {
    Close();
    if (mConverter != nullptr) mConverter->Release();
}

// 0x10000b24: Close runs through the virtual slot (at 0xc), then the converter is released (slot 8) if still held.
CStaticMBBuffer::~CStaticMBBuffer() {
    Close();
    if (mConverter != nullptr) mConverter->Release();
}

// ltms TbFontRender.cpp, the 4-bit font render method's core (unit w45a; ltms symbols shared 0x10000bf0..0x10001f64): the
// method's constructor and destructors, DrawChar (decodes one glyph and hands it to the writer for the render area's
// pixel format), CalculateSourceRect, GetRotate and the decode buffer FourBit::FontData. The nybble and bit iterators
// (include/TbFontBits.h) are inline. The writers (DrawCharTo*) and the four method objects are unit w45b's.

// ---- the destructors

// 0x10000c38: the base class's destructor (the original also deletes `this` when its hidden flag is positive: the
// compiler's deleting form).
TbFontRenderMethod::~TbFontRenderMethod() {}

// 0x10000cb4
Tb4BitFontRenderMethod::~Tb4BitFontRenderMethod() {}

namespace {

// The instance FontData::I answers (Mac: the words at 0x100116fc and 0x10011700). Its destructor runs at exit.
FourBit::FontData gFontData = {0, nullptr};

}  // namespace

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
