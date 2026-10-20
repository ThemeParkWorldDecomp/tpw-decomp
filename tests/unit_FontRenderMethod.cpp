// Test library for the 4-bit font render method's core (libtpw_unit_FontRenderMethod.dylib; see tests/test_FontRenderMethod.py,
// unit w45a): src/ltms_fontrender_method.cpp (Tb4BitFontRenderMethod's constructor-side members, DrawChar, CalculateSourceRect,
// GetRotate, FourBit::FontData, TbFontRenderMethod's destructor), src/ltms_fontrender.cpp (TbFontRender and the render parameters'
// members, among them the five of this unit), src/tb_fontrenderparams.cpp (the parameters' constructors and destructor, which
// TbFontRender's constructor runs) and src/TbIRLE4bitFont.cpp (the real font: GetCharHeader and GetCharData read a real image).
// The nybble and bit iterators are inline (include/TbFontBits.h) and are exercised through the exports below.
//
// Everything else the code calls is a fake that records through calllog, the same events as the PowerPC side's fakes:
//   the font's AddRef and Release (a real TbIRLE4bitFont over a font image the case supplies);
//   LbDraw_GetClipRect (answers a case-owned clip rectangle) and TbRect::Intersection (the formula of the listing, recording its
//   operands);
//   the six pixel writers (DrawCharTo555 .. DrawCharToBGRA of a FakeMethod): they record the area, every word of the parameter
//   record and a digest of the glyph data it points to;
//   the parameters tests' fonts (AddRef, Release, GetCaps, GetType).
// _lbpCurrentRA is defined here (the bullfrog library's variable).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "LbDraw.h"
#include "TbFontBits.h"
#include "TbFontRender.h"
#include "TbFontRenderParams.h"
#include "TbIRLE4bitFont.h"
#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- fakes of what the unit does not translate

int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}
void SamsUtilities::_USwapBlock16(uint16_t*, int32_t) {}
// The font's constructor with an index is not run; these only satisfy the link.
void* TbIResourceIndex::GetResourceItemHandle(uint32_t) { return nullptr; }
uint16_t TbIResourceIndex::GetResourceType(void*) { return 0; }
TbError TbIResourceIndex::GetPreOpenedResource(void*, void**) { return TbError(-1); }

// The render area's default constructor (the parameters' constructor makes one; the tests never run it).
TbRenderArea::TbRenderArea() {
    std::memset(this, 0, sizeof(*this));
}

TbRenderArea* _lbpCurrentRA = nullptr;
TbFontRenderMethod* gFontRenderMethods[2][5][3];

namespace {

TbRect gClip = {0, 0, 0, 0};
int32_t gExpectLength = 0;   // bytes of glyph data the writers digest (set by the case)
const uint8_t* gGlyphData = nullptr;   // where the case's glyph data starts inside the font image

}  // namespace

// 0x1002b554: the clip rectangle the case supplies.
TbRect* LbDraw_GetClipRect() {
    ev("GetClipRect");
    return &gClip;
}

// 0x10029f8c: the listing's formula, recording the operands (this, then the argument). TbRect::Intersection is tested on its own
// (tests/test_TbRectIntersection.py).
TbRect TbRect::Intersection(const TbRect& other) const {
    ev("Intersection", {left, top, right, bottom, other.left, other.top, other.right, other.bottom});
    const int32_t cutLeft = left > other.left ? left : other.left;
    const int32_t cutTop = top > other.top ? top : other.top;
    int32_t cutRight = right < other.right ? right : other.right;
    int32_t cutBottom = bottom < other.bottom ? bottom : other.bottom;
    if (cutRight < cutLeft) cutRight = cutLeft;
    if (cutBottom < cutTop) cutBottom = cutTop;
    return {cutLeft, cutTop, cutRight, cutBottom};
}

namespace {

// ---- the fonts

// A real 4-bit font over a case-supplied image; only the reference calls are recorded.
class RecFont : public TbIRLE4bitFont {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
};

// A font for the parameters' members: reference calls recorded, caps and type from the case.
class ParamFont : public TbIFont {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
    uint32_t GetCharIndex(const unsigned char*&) override { return 0; }
    uint16_t GetCharCount() override { return 0; }
    TbFontCharSize GetCharSize(uint16_t) override { return {0, 0}; }
    uint8_t GetVerticalSpacing() override { return 0; }
    uint32_t GetCaps() override { return caps; }
    uint32_t GetType() override {
        ev("GetType", {lab(this)});
        return type;
    }
    TbIMBToUnicode* GetMBToUnicode() override { return nullptr; }

    uint32_t caps = 0;
    uint32_t type = 0;
};

// A digest of `length` bytes (the writers digest what they were handed, up to 64 bytes).
uint32_t Digest(const uint8_t* bytes, int32_t length) {
    uint32_t hash = 0;
    for (int32_t i = 0; i < length && i < 64; ++i) hash = hash * 31 + bytes[i];
    return hash;
}

// ---- the render method: the six writers record the record they are given

class FakeMethod : public Tb4BitFontRenderMethod {
public:
    TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("To555", area, p); }
    TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("To565", area, p); }
    TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("To4444", area, p); }
    TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("ToAny", area, p); }
    TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("To8888", area, p); }
    TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& p) override { return Write("ToBGRA", area, p); }

private:
    // The area's label, the point, the bytes in a row, the source rectangle, the parameters' label and the width byte; then where
    // the data is (1 the decode buffer, 2 the glyph's own data in the font image, 0 elsewhere) and its digest.
    TbError Write(const char* name, TbRenderArea& area, const C4BitFontRenderMethodParameters& p) {
        ev(name, {lab(&area), p.mPoint->x, p.mPoint->y, p.mBytesPerRow, p.mSource->left, p.mSource->top, p.mSource->right,
                  p.mSource->bottom, lab(p.mParams), p.mWidth});
        int64_t kind = 0;
        if (FourBit::FontData::sMade && p.mData == FourBit::FontData::I().mBlock) kind = 1;
        else if (p.mData == gGlyphData) kind = 2;
        ev("Data", {kind, Digest(p.mData, gExpectLength), gExpectLength});
        return TbError(static_cast<int32_t>(ret("Writer", 0)));
    }
};

// ---- the case's objects

constexpr int kLabelFont = 10, kLabelArea = 20, kLabelParams = 30, kLabelMethod = 40;

std::vector<uint8_t> gImage;
alignas(RecFont) unsigned char gFontStorage[sizeof(RecFont)];
alignas(TbRenderArea) unsigned char gAreaStorage[sizeof(TbRenderArea)];
alignas(TbFontRenderParams) unsigned char gParamsStorage[sizeof(TbFontRenderParams)];
alignas(FakeMethod) unsigned char gMethodStorage[sizeof(FakeMethod)];

// A case's singleton: state 0 is "not made yet" (the size and block words are left as junk), state 1 "made" with a block of
// `size` bytes filled with 0xa5. The old block goes first, so the heap traffic of the case alone is counted.
uint8_t* SetSingleton(int32_t state, uint32_t size) {
    FourBit::FontData& data = FourBit::FontData::I();
    delete[] data.mBlock;
    data.mBlock = nullptr;
    if (state == 0) {
        FourBit::FontData::sMade = false;
        data.mSize = 0x1234abcdu;
    } else {
        FourBit::FontData::sMade = true;
        data.mSize = size;
        data.mBlock = new uint8_t[size];
        std::memset(data.mBlock, 0xa5, size);
    }
    return data.mBlock;
}

void ResetHeap() {
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
}

// Gives the memory of an object whose destructor was called by hand back without counting the operator delete (the original
// does not delete in that form).
void ReleaseQuietly(void* object) {
    const int32_t counted = tpw_cl_heap[2];
    ::operator delete(object);
    tpw_cl_heap[2] = counted;
}

}  // namespace

extern "C" {

// ---- GetRotate, CalculateSourceRect

int32_t tpw_fr_getrotate(uint32_t mask) {
    alignas(FakeMethod) static unsigned char storage[sizeof(FakeMethod)];
    FakeMethod* method = new (storage) FakeMethod;
    return method->GetRotate(mask);
}

// Tb4BitFontRenderMethod::CalculateSourceRect(rect, point, area) with the clip rectangle `clip`; the area is not read (a label only).
void tpw_fr_calcrect(const int32_t* rect, const int32_t* point, const int32_t* clip, int32_t* out) {
    FakeMethod* method = new (gMethodStorage) FakeMethod;
    gClip = {clip[0], clip[1], clip[2], clip[3]};
    const TbRect source = {rect[0], rect[1], rect[2], rect[3]};
    const TbPoint at = {point[0], point[1]};
    TbRenderArea* area = new (gAreaStorage) TbRenderArea;
    tpw_cl_label(kLabelArea, area);
    const TbRect result = method->CalculateSourceRect(source, at, *area);
    out[0] = result.left;
    out[1] = result.top;
    out[2] = result.right;
    out[3] = result.bottom;
}

// ---- the lifetime of the methods: kind 0 only makes one, 1 runs ~Tb4BitFontRenderMethod, 2 the base class's destructor, both with the hidden flag
// (a positive halfword deletes the object); this = 0 for `isNull`. out[0] is the number of operator delete calls.
void tpw_fr_method_life(int32_t kind, int32_t flag, int32_t isNull, int64_t* out) {
    ResetHeap();
    FakeMethod* method = isNull != 0 ? nullptr : new FakeMethod;
    ResetHeap();
    const int16_t half = static_cast<int16_t>(flag);
    if (kind == 0) {
        // The constructor: nothing the host can observe.
        method->~FakeMethod();
        ReleaseQuietly(method);
    } else if (method == nullptr) {
        // Deleting a NULL pointer is the original's skip of the whole body.
    } else if (half > 0) {
        delete method;
    } else {
        // (The base class's destructor is the same on the host: a virtual destructor ends in it. Calling it by name on its own
        // traps in an optimised build.)
        method->~FakeMethod();
        ReleaseQuietly(method);
    }
    out[0] = tpw_cl_heap[2];
}

// ---- FontData: kind 0 is I() from a state (as SetSingleton), 1 the destructor of a FontData holding a block of 16 bytes.
// out: the made flag, the size word, whether the block pointer changed, the new[] and delete[] counts.
void tpw_fr_fontdata(int32_t kind, int32_t state, uint32_t size, int32_t flag, int32_t isNull, int64_t* out) {
    if (kind == 0) {
        uint8_t* const before = SetSingleton(state, size);
        ResetHeap();
        FourBit::FontData& data = FourBit::FontData::I();
        out[0] = FourBit::FontData::sMade ? 1 : 0;
        out[1] = data.mSize;
        out[2] = data.mBlock != before ? 1 : 0;
        out[3] = tpw_cl_heap[1];
        out[4] = tpw_cl_heap[3];
        return;
    }
    FourBit::FontData* object = isNull != 0 ? nullptr : new FourBit::FontData{size, new uint8_t[16]};
    ResetHeap();
    const int16_t half = static_cast<int16_t>(flag);
    if (object == nullptr) {
        // The original's skip of the body for a NULL this.
    } else if (half > 0) {
        delete object;
    } else {
        object->~FontData();
        ReleaseQuietly(object);
    }
    out[0] = out[1] = out[2] = 0;
    out[3] = tpw_cl_heap[3];
    out[4] = tpw_cl_heap[2];
}

// ---- DrawChar over a font image. Fills out with: the answer, the drawn rectangle (4), the point afterwards (2), the made flag, the
// size word, whether the block moved, the area's six words afterwards, the font's mState words (4). The bytes of the decode block
// go to blockOut. Returns the number of words.
int32_t tpw_fr_drawchar(const uint8_t* image, int32_t imageLength, const uint32_t* areaWords, const int32_t* clip, int32_t x, int32_t y,
                        int32_t glyph, int32_t state, uint32_t size, int32_t glyphDataOffset, int32_t expectLength, int64_t* out,
                        uint8_t* blockOut, int32_t blockLength) {
    gImage.assign(image, image + imageLength);
    gImage.resize(imageLength + 128, 0);
    RecFont* font = new (gFontStorage) RecFont;
    font->mData = gImage.data();
    font->mHeader = gImage.data();
    font->mOffsets = reinterpret_cast<uint32_t*>(gImage.data() + 8);
    font->mMBToUnicode = nullptr;
    gGlyphData = glyphDataOffset >= 0 ? gImage.data() + glyphDataOffset : nullptr;
    gExpectLength = expectLength;
    TbRenderArea* area = new (gAreaStorage) TbRenderArea;
    std::memcpy(area->mF00, areaWords, 16);
    area->mDepth = static_cast<int32_t>(areaWords[4]);
    area->mPalettised = static_cast<int32_t>(areaWords[5]);
    _lbpCurrentRA = area;
    std::memset(gParamsStorage, 0xa5, sizeof(gParamsStorage));   // (not constructed: the words the case does not set stay 0xa5)
    TbFontRenderParams* params = reinterpret_cast<TbFontRenderParams*>(gParamsStorage);
    params->mState[0] = reinterpret_cast<uintptr_t>(font);
    FakeMethod* method = new (gMethodStorage) FakeMethod;
    gClip = {clip[0], clip[1], clip[2], clip[3]};
    tpw_cl_label(kLabelFont, font);
    tpw_cl_label(kLabelArea, area);
    tpw_cl_label(kLabelParams, params);
    tpw_cl_label(kLabelMethod, method);
    uint8_t* const before = SetSingleton(state, size);
    ResetHeap();
    const TbPoint at = {x, y};
    TbRect drawn = {-1, -2, -3, -4};
    const TbError result = method->DrawChar(at, static_cast<uint16_t>(glyph), drawn, *params);
    int32_t n = 0;
    out[n++] = static_cast<int32_t>(result);
    out[n++] = drawn.left;
    out[n++] = drawn.top;
    out[n++] = drawn.right;
    out[n++] = drawn.bottom;
    out[n++] = at.x;
    out[n++] = at.y;
    const bool made = FourBit::FontData::sMade;
    FourBit::FontData* const data = made ? &FourBit::FontData::I() : nullptr;   // (I() makes nothing once the flag is set)
    out[n++] = made ? 1 : 0;
    out[n++] = made ? data->mSize : 0;
    out[n++] = made && data->mBlock != before ? 1 : 0;
    if (made && blockLength > 0) std::memcpy(blockOut, data->mBlock, blockLength);
    for (int i = 0; i < 4; ++i) {
        uint32_t word;
        std::memcpy(&word, area->mF00 + 4 * i, 4);
        out[n++] = word;
    }
    out[n++] = static_cast<uint32_t>(area->mDepth);
    out[n++] = static_cast<uint32_t>(area->mPalettised);
    out[n++] = lab(reinterpret_cast<const void*>(params->mState[0]));
    out[n++] = static_cast<uint32_t>(params->mState[1]);
    out[n++] = static_cast<uint32_t>(params->mState[2]);
    out[n++] = static_cast<uint32_t>(params->mState[3]);
    return n;
}

// ---- the iterators: a script. buffer: 64 bytes the readers read; the writers write into a second 64 bytes, filled with 0xa5. Each
// operation is four words (kind, a, b, c); kinds:
//   0 NybbleIterator(i = a, offset b, low c)   1 *iterator a   2 iterator a ++ (answers the old position)   3 current of a
//   4 NybbleWriter(j = a, offset b, countOnly c)   5 Write(a, nybble b)   6 Nybbles of a
//   7 BitIterator(k = a, offset b, bit c)   8 *bit iterator a, and its conversion   9 bit iterator a ++ (the old position)
// out gets, after each operation, the answer words (2) then the dump of the three nybble iterators (flag, offset), the two writers
// (offset, second, countOnly, count) and the three bit iterators (mask, offset), then the 64 output bytes. Returns the words.
constexpr int kIters = 3, kWriters = 2, kBits = 3;

int32_t tpw_fr_iterators(const uint8_t* buffer, const int32_t* ops, int32_t nOps, int64_t* out) {
    static uint8_t readBuffer[64];
    static uint8_t writeBuffer[64];
    std::memcpy(readBuffer, buffer, 64);
    std::memset(writeBuffer, 0xa5, 64);
    FourBit::NybbleIterator iterators[kIters] = {{readBuffer, false}, {readBuffer, false}, {readBuffer, false}};
    FourBit::NybbleWriter writers[kWriters] = {{writeBuffer, false}, {writeBuffer, false}};
    OneBit::BitIterator bits[kBits] = {{readBuffer, 0}, {readBuffer, 0}, {readBuffer, 0}};
    int32_t n = 0;
    for (int32_t op = 0; op < nOps; ++op) {
        const int32_t kind = ops[4 * op], a = ops[4 * op + 1], b = ops[4 * op + 2], c = ops[4 * op + 3];
        int64_t answer0 = 0, answer1 = 0;
        switch (kind) {
            case 0: iterators[a] = FourBit::NybbleIterator(readBuffer + b, c != 0); break;
            case 1: answer0 = *iterators[a]; break;
            case 2: {
                FourBit::NybbleIterator old = iterators[a]++;
                answer0 = old.mLow ? 1 : 0;
                answer1 = old.mPointer - readBuffer;
                break;
            }
            case 3: answer0 = iterators[a].current(); break;
            case 4: writers[a] = FourBit::NybbleWriter(writeBuffer + b, c != 0); break;
            case 5: writers[a].Write(static_cast<uint8_t>(b)); break;
            case 6: answer0 = writers[a].Nybbles(); break;
            case 7: bits[a] = OneBit::BitIterator(readBuffer + b, static_cast<uint8_t>(c)); break;
            case 8: {
                const OneBit::BitIterator::BitReference reference = *bits[a];
                answer0 = reference.mMask;
                answer1 = static_cast<uint8_t>(reference);
                break;
            }
            case 9: {
                OneBit::BitIterator old = bits[a]++;
                answer0 = old.mMask;
                answer1 = old.mPointer - readBuffer;
                break;
            }
        }
        out[n++] = answer0;
        out[n++] = answer1;
        for (const auto& it : iterators) {
            out[n++] = it.mLow ? 1 : 0;
            out[n++] = it.mPointer - readBuffer;
        }
        for (const auto& w : writers) {
            out[n++] = w.mPointer - writeBuffer;
            out[n++] = w.mSecond ? 1 : 0;
            out[n++] = w.mCountOnly ? 1 : 0;
            out[n++] = w.mCount;
        }
        for (const auto& bit : bits) {
            out[n++] = bit.mMask;
            out[n++] = bit.mPointer - readBuffer;
        }
        for (int i = 0; i < 64; ++i) out[n++] = writeBuffer[i];
    }
    return n;
}

// ---- the parameters' members: a script on three parameters objects (storage filled with a byte, never constructed) and three
// fonts. Each operation is eight words (kind, a..g):
//   0 fill object a with byte b          1 poke Mac word b (the offset / 4) of object a with c (word 1 is the font: -1 for none, else a
//     font number; 2 caps, 3 style, 4 blend, 5..14 the ten words of the render area, 15 and 16 the point, 17 the five flag bytes
//     (the low five bytes of c), 19 the last word)   2 object a = object b
//   3 UseTexture on a with the area words stored by kind 4 and the point (b, c)   4 store the area words (b..g, the first six; the
//     other four are 0x7000 + i)   5 UseBlendedMode on a   6 GetStyle on a   7 GetType on a   8 configure font a: caps b, type c
// The dump after each operation: the answer, then each object's words: the font's label, +8, +0xc, +0x10, the ten area words, +0x3c,
// +0x40, the five flag bytes, +0x4c.
constexpr int kParamObjects = 3, kParamFonts = 3;

namespace {

ParamFont gParamFonts[kParamFonts];
alignas(TbFontRenderParams) unsigned char gParamStorage[kParamObjects][sizeof(TbFontRenderParams) + 32];
uint32_t gAreaStore[10];

TbFontRenderParams* ParamObject(int i) { return reinterpret_cast<TbFontRenderParams*>(gParamStorage[i]); }

// The ten Mac words of the render area held in the host slots 4..9 (a TbRenderArea): mapped field by field.
void AreaWords(const TbFontRenderParams& p, uint32_t* words) {
    const TbRenderArea& area = *reinterpret_cast<const TbRenderArea*>(&p.mState[4]);
    for (int i = 0; i < 4; ++i) std::memcpy(&words[i], area.mF00 + 4 * i, 4);
    words[4] = static_cast<uint32_t>(area.mDepth);
    words[5] = static_cast<uint32_t>(area.mPalettised);
    words[6] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(area.mData));
    words[7] = static_cast<uint32_t>(area.mWidth);
    words[8] = static_cast<uint32_t>(area.mPictureHeight);
    words[9] = static_cast<uint32_t>(area.mStride);
}

void SetAreaWord(TbFontRenderParams& p, int index, uint32_t value) {
    TbRenderArea& area = *reinterpret_cast<TbRenderArea*>(&p.mState[4]);
    if (index < 4) std::memcpy(area.mF00 + 4 * index, &value, 4);
    else if (index == 4) area.mDepth = static_cast<int32_t>(value);
    else if (index == 5) area.mPalettised = static_cast<int32_t>(value);
    else if (index == 6) area.mData = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(value));
    else if (index == 7) area.mWidth = static_cast<int32_t>(value);
    else if (index == 8) area.mPictureHeight = static_cast<int32_t>(value);
    else area.mStride = static_cast<int32_t>(value);
}

}  // namespace

int32_t tpw_fr_params(const int64_t* ops, int32_t nOps, int64_t* out) {
    for (int i = 0; i < kParamObjects; ++i) std::memset(gParamStorage[i], 0xa5, sizeof(gParamStorage[i]));
    for (int i = 0; i < kParamFonts; ++i) tpw_cl_label(10 + i, &gParamFonts[i]);
    for (int i = 0; i < kParamObjects; ++i) tpw_cl_label(30 + i, gParamStorage[i]);
    for (int i = 0; i < 10; ++i) gAreaStore[i] = 0x7000u + i;
    int32_t n = 0;
    for (int32_t op = 0; op < nOps; ++op) {
        const int64_t* o = ops + 8 * op;
        const int64_t kind = o[0], a = o[1], b = o[2], c = o[3];
        int64_t answer = 0;
        switch (kind) {
            case 0: std::memset(gParamStorage[a], static_cast<int>(b), sizeof(gParamStorage[a])); break;
            case 1: {
                TbFontRenderParams& p = *ParamObject(a);
                if (b == 1) p.mState[0] = c < 0 ? 0 : reinterpret_cast<uintptr_t>(&gParamFonts[c]);
                else if (b == 2) p.mState[1] = static_cast<uint32_t>(c);
                else if (b == 3) p.mState[2] = static_cast<uint32_t>(c);
                else if (b == 4) p.mState[3] = static_cast<uint32_t>(c);
                else if (b >= 5 && b <= 14) SetAreaWord(p, static_cast<int>(b - 5), static_cast<uint32_t>(c));
                else if (b == 15) p.mState[14] = static_cast<uint32_t>(c);
                else if (b == 16) p.mState[15] = static_cast<uint32_t>(c);
                else if (b == 17) std::memcpy(&p.mState[16], &c, 5);
                else if (b == 19) p.mState[18] = static_cast<uint32_t>(c);
                break;
            }
            case 2: *ParamObject(a) = *ParamObject(b); break;
            case 3: {
                TbRenderArea area;
                std::memset(&area, 0, sizeof(area));
                std::memcpy(area.mF00, &gAreaStore[0], 16);
                area.mDepth = static_cast<int32_t>(gAreaStore[4]);
                area.mPalettised = static_cast<int32_t>(gAreaStore[5]);
                area.mData = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(gAreaStore[6]));
                area.mWidth = static_cast<int32_t>(gAreaStore[7]);
                area.mPictureHeight = static_cast<int32_t>(gAreaStore[8]);
                area.mStride = static_cast<int32_t>(gAreaStore[9]);
                const TbPoint point = {static_cast<int32_t>(b), static_cast<int32_t>(c)};
                answer = ParamObject(a)->UseTexture(area, point);
                break;
            }
            case 4:
                for (int i = 0; i < 6; ++i) gAreaStore[i] = static_cast<uint32_t>(o[2 + i]);
                break;
            case 5: answer = ParamObject(a)->UseBlendedMode(); break;
            case 6: answer = ParamObject(a)->GetStyle(); break;
            case 7: answer = ParamObject(a)->GetType(); break;
            case 8:
                gParamFonts[a].caps = static_cast<uint32_t>(b);
                gParamFonts[a].type = static_cast<uint32_t>(c);
                break;
        }
        out[n++] = answer;
        for (int i = 0; i < kParamObjects; ++i) {
            const TbFontRenderParams& p = *ParamObject(i);
            out[n++] = lab(reinterpret_cast<const void*>(p.mState[0]));
            out[n++] = static_cast<uint32_t>(p.mState[1]);
            out[n++] = static_cast<uint32_t>(p.mState[2]);
            out[n++] = static_cast<uint32_t>(p.mState[3]);
            uint32_t words[10];
            AreaWords(p, words);
            for (uint32_t word : words) out[n++] = word;
            out[n++] = static_cast<uint32_t>(p.mState[14]);
            out[n++] = static_cast<uint32_t>(p.mState[15]);
            const uint8_t* flags = reinterpret_cast<const uint8_t*>(&p.mState[16]);
            for (int f = 0; f < 5; ++f) out[n++] = flags[f];
            out[n++] = static_cast<uint32_t>(p.mState[18]);
        }
    }
    return n;
}

}  // extern "C"
