// Test library for the ltms text formatter and font renderer (libtpw_unit_TextFormatter.dylib; see tests/test_TextFormatter.py):
// src/ltms_textformatter.cpp (TbTextFormatter and the three vector tear-downs), src/ltms_fontrender.cpp (TbFontRender and the
// TbFontRenderParams members, among them operator=, GetStyle and GetType of unit w45a) and the real src/tb_fontrenderparams.cpp (the parameters' constructors and destructor, which the
// formatter's render runs).
//
// Everything else they call is a fake that records through calllog, the same events as the PowerPC side's fakes:
//   the fonts (three FakeFont objects: AddRef, Release, GetCharSize, GetVerticalSpacing, GetCaps, GetType, GetMBToUnicode) and
//   the three multibyte converters they hand out (real TbIMBToUnicode objects over host tables);
//   the render methods (thirty FakeMethod objects behind gFontRenderMethods): DrawChar writes a rectangle that depends on the
//   glyph and the position;
//   the vectors' clear, push_back and resize (a growing block doubles from 1 as the Metrowerks vector's, and is a labelled
//   operator new block, so that the tear-down's delete shows in the log);
//   ProcessLine (reverses or rotates a line on request), TbRect::Bounding and the render area's constructor.
//
// The Python module drives the library through tpw_tf_op on a small world: two formatters, three parameter objects, two
// stand-alone renders, three fonts and three converters, and strings it hands in. After each operation it reads each live
// object as a flat list of numbers (tpw_tf_dump), in the order the module's own dump reads the original's.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>
#include <vector>

#include "calllog.h"
#include "TbFontRender.h"
#include "TbFontRenderParams.h"
#include "TbResource.h"
#include "TbTextFormatter.h"

using namespace calllog;

namespace {

constexpr int kFonts = 3;
constexpr int kConverters = 3;
constexpr int kFormatters = 2;
constexpr int kParams = 3;
constexpr int kRenders = 2;
constexpr int kMethods = 30;
constexpr int kTables = 4096;     // halfwords of a converter's table
constexpr std::size_t kGuard = 32;

// Fixed labels (the first labels made during a case start at 100): the fonts 10 to 12 and the methods 60 to 89 are labelled by the
// Python module (they are static); the objects the operations make are labelled here as they are made.
constexpr int kLabelFormatter = 20, kLabelFormatterParams = 30, kLabelParams = 40, kLabelRender = 50, kLabelRenderParams = 54;

bool gQuiet = false;           // the per-character events (GetCharSize, push_back) are not recorded
int32_t gMethodMode = 0;       // what the fake methods draw: 0 by the glyph, 1 nothing (the rectangle is left), 2 empty, 3 full

// ---- the converters

alignas(TbIMBToUnicode) unsigned char gConverterStorage[kConverters][sizeof(TbIMBToUnicode)];
uint16_t gTables[kConverters][kTables];
bool gConverterMade[kConverters];

TbIMBToUnicode* Converter(int index) { return reinterpret_cast<TbIMBToUnicode*>(gConverterStorage[index]); }

// ---- the fonts

class FakeFont : public TbIFont {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
    uint32_t GetCharIndex(const unsigned char*&) override { return 0; }
    uint16_t GetCharCount() override { return 0; }
    // The width is the table's (0 past it) or wbase + (index * wmul + number * 5) % wmod; the second word is a mark nothing should use.
    TbFontCharSize GetCharSize(uint16_t index) override {
        if (!gQuiet) ev("GetCharSize", {lab(this), index});
        if (nwidths != 0) return {index < nwidths ? widths[index] : 0u, 0x7000u + index};
        return {wbase + (static_cast<uint32_t>(index) * wmul + static_cast<uint32_t>(number) * 5) % wmod, 0x7000u + index};
    }
    uint8_t GetVerticalSpacing() override {
        ev("GetVerticalSpacing", {lab(this)});
        return spacing;
    }
    uint32_t GetCaps() override {
        ev("GetCaps", {lab(this)});
        return caps;
    }
    uint32_t GetType() override {
        ev("GetType", {lab(this)});
        return type;
    }
    TbIMBToUnicode* GetMBToUnicode() override {
        ev("GetMBToUnicode", {lab(this)});
        return converter >= 0 && gConverterMade[converter] ? Converter(converter) : nullptr;
    }

    int number = 0;
    uint32_t widths[1024];      // a table of widths by index (a real font's), used when nwidths is not 0
    uint32_t nwidths = 0;
    uint8_t spacing = 0;
    uint32_t caps = 0;
    uint32_t type = 0;
    int converter = -1;
    uint32_t wbase = 0;
    uint32_t wmod = 1;
    uint32_t wmul = 1;
};
FakeFont gFonts[kFonts];

// ---- the render methods

// What the fake methods write into the rectangle for a glyph at a position: a different shape for each glyph modulo 7, among them
// the empty rectangle and the two half-empty ones (left == right only, top == bottom only).
TbRect RectFor(uint16_t glyph, int32_t x, int32_t y) {
    const int32_t w = 3 + glyph % 9;
    const int32_t h = 5 + glyph % 4;
    switch (glyph % 7) {
        case 3: return {x, y, x, y};
        case 5: return {x, y, x, y + h};
        case 6: return {x, y, x + w, y};
        case 0: return {x, y, x + w, y + h};
        default: return {x + 1, y - 2, x + w + 3, y + h};
    }
}

class FakeMethod : public TbFontRenderMethod {
public:
    TbError DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn, TbFontRenderParams& params) override {
        ev("Method::DrawChar", {lab(this), at.x, at.y, glyph, lab(&params)});
        if (gMethodMode == 0) drawn = RectFor(glyph, at.x, at.y);
        else if (gMethodMode == 2) drawn = {at.x, at.y, at.x, at.y};
        else if (gMethodMode == 3) drawn = {at.x, at.y, at.x + 5, at.y + 6};
        return TbError(static_cast<int32_t>(ret("MethodDrawChar", 0)));
    }
};
FakeMethod gMethods[kMethods];

// ---- the objects the operations work on

TbTextFormatter* gFormatter[kFormatters];
TbFontRenderParams* gParams[kParams];
TbFontRender* gRender[kRenders];
std::vector<std::vector<uint8_t>> gTexts;

std::size_t SizeOf(int what) {
    return what == 0 ? sizeof(TbTextFormatter) : what == 1 ? sizeof(TbFontRenderParams) : sizeof(TbFontRender);
}

// The params a reference names: 0..2 a stand-alone object, 100 and 101 a formatter's own.
TbFontRenderParams* ParamsOf(int ref) {
    if (ref >= 200) return &gRender[ref - 200]->mParams;
    if (ref >= 100) return &gFormatter[ref - 100]->mRender.mParams;
    return gParams[ref];
}

// The formatter a vector belongs to, or -1.
int FormatterOf(const void* address) {
    const char* p = static_cast<const char*>(address);
    for (int i = 0; i < kFormatters; ++i) {
        const char* base = reinterpret_cast<const char*>(gFormatter[i]);
        if (base != nullptr && p >= base && p < base + sizeof(TbTextFormatter)) return i;
    }
    return -1;
}

int64_t OwnerLabel(const void* vector) {
    const int i = FormatterOf(vector);
    return i < 0 ? 0 : kLabelFormatter + i;
}

// The index of an element pointer in its formatter's character list (0 without one).
int64_t CharIndex(const void* owner, const CCharacterInfo* element) {
    const TbTextFormatter* formatter = nullptr;
    const int i = FormatterOf(owner);
    if (i >= 0) formatter = gFormatter[i];
    if (formatter == nullptr || formatter->mChars.mData == nullptr) return 0;
    return element - formatter->mChars.mData;
}

// A block the fakes make: operator new (counted) with a gap behind it, so that a pointer one past the end of one block is never the
// start of another (the line records of the formatter point into the character list, and the dumps place them by that).
constexpr std::size_t kGap = 32;

// What operator delete does for a block of the fakes (it counts, and logs the delete of a labelled one) but the memory is kept: a line
// record left pointing into a block the vector has outgrown must not point into a later block on the host, as it cannot on the PowerPC
// side (whose blocks are never reused).
void Retire(void* block) {
    ++tpw_cl_heap[2];
    if (calllog::on() && calllog::lab(block) > 0) {
        calllog::ev("delete", {calllog::lab(block)});
        calllog::forget(block);
    }
}

// Room for one more element: the block doubles from 1 (the original's push_back), the old one is let go after the copy.
template <class T>
void Grow(MslVector<T>& vector) {
    if (vector.mSize < vector.mCapacity) return;
    const uint32_t capacity = vector.mCapacity == 0 ? 1 : vector.mCapacity * 2;
    T* block = static_cast<T*>(::operator new(capacity * sizeof(T) + kGap));
    ev("alloc", {static_cast<int64_t>(capacity), newlab(block)});
    std::memset(block, 0xa5, capacity * sizeof(T));
    if (vector.mSize != 0) std::memcpy(block, vector.mData, vector.mSize * sizeof(T));
    T* old = vector.mData;
    vector.mData = block;
    if (old != nullptr) Retire(old);
    vector.mCapacity = capacity;
}

}  // namespace

// ---- fakes of the classes the unit does not translate

TbRenderArea::TbRenderArea() {
    ev("TbRenderArea::TbRenderArea", {newlab(this)});
    static_assert(sizeof(TbRenderArea) >= 40, "the ten Mac words must fit");
    for (uint32_t i = 0; i < 10; ++i) {
        const uint32_t word = 0x7a000000u + i;
        std::memcpy(reinterpret_cast<uint8_t*>(this) + 4 * i, &word, 4);
    }
}
int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
TbIFont::~TbIFont() {}
TbIMBInfo::~TbIMBInfo() {}
TbIMBToUnicode::~TbIMBToUnicode() {}
TbFontRenderMethod::~TbFontRenderMethod() {}

TbFontRenderMethod* gFontRenderMethods[2][5][3];

TbRect TbRect::Bounding(const TbRect& other) const {
    ev("Bounding", {left, top, right, bottom, other.left, other.top, other.right, other.bottom});
    return {std::min(left, other.left), std::min(top, other.top), std::max(right, other.right), std::max(bottom, other.bottom)};
}

// The vectors' untranslated members. The events carry the kind (0 characters, 1 lines, 2 points) and the formatter's label.
template <>
void MslVector<CCharacterInfo>::clear() {
    ev("clear", {0, OwnerLabel(this), mSize});
    mSize = 0;
}
template <>
void MslVector<CLineInfo>::clear() {
    ev("clear", {1, OwnerLabel(this), mSize});
    mSize = 0;
}
template <>
void MslVector<TbPoint>::clear() {
    ev("clear", {2, OwnerLabel(this), mSize});
    mSize = 0;
}
template <>
void MslVector<CCharacterInfo>::push_back(const CCharacterInfo& element) {
    if (!gQuiet) ev("push_back", {0, OwnerLabel(this), mSize, element.mChar, element.mIndex, element.mType, element.mWidth, element.mPosition});
    Grow(*this);
    mData[mSize] = element;
    ++mSize;
}
template <>
void MslVector<CLineInfo>::push_back(const CLineInfo& element) {
    ev("push_back", {1, OwnerLabel(this), mSize, CharIndex(this, element.mBegin), CharIndex(this, element.mEnd), element.mWidth});
    Grow(*this);
    mData[mSize] = element;
    ++mSize;
}
// Grows to exactly the size asked for when the block is too small; the new entries are zero points.
template <>
void MslVector<TbPoint>::resize(uint32_t size) {
    ev("resize", {2, OwnerLabel(this), size, mSize});
    if (size > mSize) {
        if (size > mCapacity) {
            TbPoint* block = static_cast<TbPoint*>(::operator new(size * sizeof(TbPoint) + kGap));
            ev("alloc", {static_cast<int64_t>(size), newlab(block)});
            if (mSize != 0) std::memcpy(block, mData, mSize * sizeof(TbPoint));
            TbPoint* old = mData;
            mData = block;
            if (old != nullptr) Retire(old);
            mCapacity = size;
        }
        for (uint32_t i = mSize; i < size; ++i) mData[i] = TbPoint{0, 0};
    }
    mSize = size;
}

// The line processor: mode 0 leaves the line, 1 reverses it, 2 rotates it left by one (the queue "ProcessLine", the last repeats).
void ProcessLine(CCharacterInfo* first, CCharacterInfo* last) {
    const int64_t mode = ret("ProcessLine", 0);
    int owner = -1;
    for (int i = 0; i < kFormatters; ++i) {
        const TbTextFormatter* f = gFormatter[i];
        if (f != nullptr && f->mChars.mData != nullptr && first >= f->mChars.mData && last <= f->mChars.mData + f->mChars.mSize) owner = i;
    }
    const TbTextFormatter* f = owner >= 0 ? gFormatter[owner] : nullptr;
    ev("ProcessLine", {owner >= 0 ? kLabelFormatter + owner : 0, f ? first - f->mChars.mData : 0, f ? last - f->mChars.mData : 0, mode});
    if (mode == 1) std::reverse(first, last);
    else if (mode == 2 && last - first >= 2) std::rotate(first, first + 1, last);
}

namespace {

// ---- the dumps

void Put(std::vector<int64_t>& out, int64_t value) { out.push_back(value); }

void DumpParams(const TbFontRenderParams* p, std::vector<int64_t>& out) {
    Put(out, lab(reinterpret_cast<const void*>(p->mState[0])));
    Put(out, static_cast<uint32_t>(p->mState[1]));
    Put(out, static_cast<uint32_t>(p->mState[2]));
    Put(out, static_cast<uint32_t>(p->mState[3]));
    uint32_t area[10];
    std::memcpy(area, &p->mState[4], sizeof(area));
    for (uint32_t word : area) Put(out, word);
    Put(out, static_cast<uint32_t>(p->mState[14]));
    Put(out, static_cast<uint32_t>(p->mState[15]));
    const uint8_t* flags = reinterpret_cast<const uint8_t*>(&p->mState[16]);
    for (int i = 0; i < 5; ++i) Put(out, flags[i]);
    Put(out, static_cast<uint32_t>(p->mState[18]));
}

template <class T>
void DumpVectorHeader(const MslVector<T>& v, std::vector<int64_t>& out) {
    Put(out, v.mCapacity);
    Put(out, v.mSize);
    Put(out, lab(v.mData));
}

// A pointer as an element number of the character list, or -1 when it is not inside it (a line left over from a list that has gone).
int64_t IndexIn(const MslVector<CCharacterInfo>& chars, const CCharacterInfo* pointer) {
    if (chars.mData == nullptr || pointer < chars.mData || pointer > chars.mData + chars.mSize) return -1;
    return pointer - chars.mData;
}

// The elements of a vector are written only when its block is one the test made (a labelled one): a poked pointer is not followed.
void DumpFormatter(const TbTextFormatter* f, std::vector<int64_t>& out) {
    Put(out, lab(f->mRender.mMethod));
    DumpParams(&f->mRender.mParams, out);
    DumpVectorHeader(f->mChars, out);
    for (uint32_t i = 0; i < f->mChars.mSize && lab(f->mChars.mData) > 0; ++i) {
        const CCharacterInfo& c = f->mChars.mData[i];
        Put(out, c.mChar);
        Put(out, c.mIndex);
        Put(out, c.mType);
        Put(out, c.mWidth);
        Put(out, c.mPosition);
    }
    Put(out, f->mCharCounter);
    DumpVectorHeader(f->mLines, out);
    for (uint32_t i = 0; i < f->mLines.mSize && lab(f->mLines.mData) > 0; ++i) {
        const CLineInfo& line = f->mLines.mData[i];
        Put(out, IndexIn(f->mChars, line.mBegin));
        Put(out, IndexIn(f->mChars, line.mEnd));
        Put(out, line.mWidth);
    }
    Put(out, f->mMaxLineWidth);
    DumpVectorHeader(f->mPositions, out);
    for (uint32_t i = 0; i < f->mPositions.mSize && lab(f->mPositions.mData) > 0; ++i) {
        Put(out, f->mPositions.mData[i].x);
        Put(out, f->mPositions.mData[i].y);
    }
    Put(out, f->mMeasurePositions);
    Put(out, f->mCaret.x);
    Put(out, f->mCaret.y);
    Put(out, f->mVerticalJustification);
    Put(out, f->mHorizontalJustification);
    Put(out, f->mWrapWidth);
    Put(out, f->mBounds.left);
    Put(out, f->mBounds.top);
    Put(out, f->mBounds.right);
    Put(out, f->mBounds.bottom);
    Put(out, f->mLineHeight);
}

void DumpRender(const TbFontRender* r, std::vector<int64_t>& out) {
    Put(out, lab(r->mMethod));
    DumpParams(&r->mParams, out);
}

// A kind of object's storage: malloc, filled with `fill`, a guard behind it.
void* MakeStorage(int what, int fill) {
    void* p = std::malloc(SizeOf(what) + kGuard);
    std::memset(p, fill, SizeOf(what) + kGuard);
    return p;
}

}  // namespace

extern "C" {

// ---- the world

// Forgets the objects of the last case and frees their storage; the converters, fonts and methods are made again by the case.
void tpw_tf_reset() {
    for (auto& f : gFormatter) { std::free(f); f = nullptr; }
    for (auto& p : gParams) { std::free(p); p = nullptr; }
    for (auto& r : gRender) { std::free(r); r = nullptr; }
    gTexts.clear();
    gQuiet = false;
    gMethodMode = 0;
    for (int i = 0; i < kConverters; ++i) gConverterMade[i] = false;
    for (auto& row : gFontRenderMethods) for (auto& cell : row) for (auto& method : cell) method = nullptr;
}

// Storage for one object of a kind (0 formatter, 1 parameters, 2 render), at an index, filled with `fill`; labelled by the caller.
void* tpw_tf_storage(int32_t what, int32_t index, int32_t fill) {
    void** slot = what == 0 ? reinterpret_cast<void**>(&gFormatter[index]) : what == 1 ? reinterpret_cast<void**>(&gParams[index])
                                                                                       : reinterpret_cast<void**>(&gRender[index]);
    std::free(*slot);
    *slot = MakeStorage(what, fill);
    return *slot;
}
void* tpw_tf_object(int32_t what, int32_t index) {
    return what == 0 ? static_cast<void*>(gFormatter[index]) : what == 1 ? static_cast<void*>(gParams[index]) : static_cast<void*>(gRender[index]);
}
void* tpw_tf_font(int32_t index) { return &gFonts[index]; }
void* tpw_tf_method(int32_t index) { return &gMethods[index]; }

void tpw_tf_configure_font(int32_t index, int32_t spacing, uint32_t caps, uint32_t type, int32_t converter, uint32_t wbase, uint32_t wmod,
                           uint32_t wmul) {
    FakeFont& f = gFonts[index];
    f.number = index;
    f.spacing = static_cast<uint8_t>(spacing);
    f.caps = caps;
    f.type = type;
    f.converter = converter;
    f.wbase = wbase;
    f.wmod = wmod;
    f.wmul = wmul;
}

void tpw_tf_configure_widths(int32_t index, const uint32_t* widths, int32_t n) {
    FakeFont& f = gFonts[index];
    f.nwidths = static_cast<uint32_t>(n);
    for (int32_t i = 0; i < n && i < 1024; ++i) f.widths[i] = widths[i];
}

void tpw_tf_configure_converter(int32_t index, int32_t first_lead, int32_t count, const uint16_t* table, int32_t n) {
    std::memset(gTables[index], 0, sizeof(gTables[index]));
    std::memcpy(gTables[index], table, static_cast<std::size_t>(n) * 2);
    new (gConverterStorage[index]) TbIMBToUnicode(static_cast<uint8_t>(first_lead), static_cast<uint16_t>(count), gTables[index]);
    gConverterMade[index] = true;
}

// The thirty cells of the method table, as numbers 0 (no method) or 1..30 (the method of that number).
void tpw_tf_configure_table(const int32_t* cells) {
    TbFontRenderMethod** flat = &gFontRenderMethods[0][0][0];
    for (int i = 0; i < 30; ++i) flat[i] = cells[i] == 0 ? nullptr : &gMethods[cells[i] - 1];
}

void tpw_tf_set_text(int32_t id, const uint8_t* bytes, int32_t n) {
    if (static_cast<int32_t>(gTexts.size()) <= id) gTexts.resize(id + 1);
    gTexts[id].assign(bytes, bytes + n);
}

void tpw_tf_set_mode(int32_t quiet, int32_t method_mode) {
    gQuiet = quiet != 0;
    gMethodMode = method_mode;
}

// Replaces the formatter's characters by `n` made ones: five numbers each (character, index, type, width, position), in a
// malloc block (not counted as heap traffic) labelled like the vector's own. The old block stays where it is.
void tpw_tf_set_chars(int32_t index, const int32_t* spec, int32_t n) {
    TbTextFormatter* f = gFormatter[index];
    CCharacterInfo* block = static_cast<CCharacterInfo*>(std::malloc((n == 0 ? 1 : n) * sizeof(CCharacterInfo) + kGap));
    newlab(block);
    for (int32_t i = 0; i < n; ++i) {
        CCharacterInfo c;
        std::memset(&c, 0, sizeof(c));
        c.mChar = static_cast<uint16_t>(spec[5 * i]);
        c.mIndex = static_cast<uint16_t>(spec[5 * i + 1]);
        c.mType = static_cast<uint8_t>(spec[5 * i + 2]);
        c.mWidth = spec[5 * i + 3];
        c.mPosition = static_cast<uint16_t>(spec[5 * i + 4]);
        block[i] = c;
    }
    f->mChars.mData = block;
    f->mChars.mSize = static_cast<uint32_t>(n);
    f->mChars.mCapacity = static_cast<uint32_t>(n);
}

// Sets one raw field of an object. what 0: a formatter field by number; 1: a parameters slot (the Mac's word number, the value as
// the Mac's 32-bit word; the font slot takes a font number or -1); 2: a vector's word of a formatter (vector * 3 + word).
void tpw_tf_poke(int32_t what, int32_t index, int32_t field, int64_t value) {
    if (what == 0) {
        TbTextFormatter* f = gFormatter[index];
        switch (field) {
            case 0: f->mMeasurePositions = static_cast<uint8_t>(value); break;
            case 1: f->mVerticalJustification = static_cast<int32_t>(value); break;
            case 2: f->mHorizontalJustification = static_cast<int32_t>(value); break;
            case 3: f->mWrapWidth = static_cast<uint32_t>(value); break;
            case 4: f->mLineHeight = static_cast<int32_t>(value); break;
            case 5: f->mBounds.left = static_cast<int32_t>(value); break;
            case 6: f->mBounds.top = static_cast<int32_t>(value); break;
            case 7: f->mBounds.right = static_cast<int32_t>(value); break;
            case 8: f->mBounds.bottom = static_cast<int32_t>(value); break;
            case 9: f->mCharCounter = static_cast<uint16_t>(value); break;
            case 10: f->mMaxLineWidth = static_cast<uint32_t>(value); break;
        }
    } else if (what == 1) {
        TbFontRenderParams* p = ParamsOf(index);
        if (field == 0) p->mState[0] = value < 0 ? 0 : reinterpret_cast<uintptr_t>(&gFonts[value]);
        else if (field == 16) std::memcpy(&p->mState[16], &value, 5);
        else p->mState[field] = static_cast<uint32_t>(value);
    } else if (what == 2) {
        TbTextFormatter* f = gFormatter[index];
        const int vector = field / 3, word = field % 3;
        uint32_t* words = vector == 0 ? &f->mChars.mCapacity : vector == 1 ? &f->mLines.mCapacity : &f->mPositions.mCapacity;
        if (word < 2) words[word] = static_cast<uint32_t>(value);
        else if (vector == 0) f->mChars.mData = reinterpret_cast<CCharacterInfo*>(value);
        else if (vector == 1) f->mLines.mData = reinterpret_cast<CLineInfo*>(value);
        else f->mPositions.mData = reinterpret_cast<TbPoint*>(value);
    } else if (what == 3) {
        TbFontRender* r = gRender[index];
        if (field == 0) r->mMethod = value == 0 ? nullptr : &gMethods[value - 1];
    }
}

// ---- the dumps

int32_t tpw_tf_dump(int32_t what, int32_t index, int64_t* out, int32_t max) {
    std::vector<int64_t> values;
    if (what == 0) DumpFormatter(gFormatter[index], values);
    else if (what == 1) DumpParams(gParams[index], values);
    else if (what == 2) DumpRender(gRender[index], values);
    const int32_t n = static_cast<int32_t>(values.size());
    for (int32_t i = 0; i < n && i < max; ++i) out[i] = values[i];
    return n;
}

// ---- the operations
//
// tpw_tf_op(op, a, out): `a` holds eight numbers, `out` gets eight. The first argument names the object (a formatter, a render or a
// parameters object by its number). The arguments after it, by operation:
//   formatter constructor a1 fill byte | destructor a1 flag (positive: the deleting form) | SetVertical, SetHorizontal a1 value
//   GetCharPositionArray, GetCharPositionArraySize | GetCharIdxFromPosition a1 x, a2 y | GetCharType a1 character
//   CreateInfoList a1 text number, a2 parameters | Layout, EmptyStringProcess a1 left, a2 top, a3 right, a4 bottom
//   LayoutLines a1 first index, a2 end index | DrawText a1 parameters, a2 bounds wanted, a3 draw
//   Draw, Measure a1..a4 the rectangle, a5 text number, a6 parameters, a7 output rectangle wanted
//   render constructor a1 fill | destructor a1 flag | Initialise a1 parameters | DrawChar a1 x, a2 y, a3 glyph
//   parameters constructor a1 fill | destructor a1 flag | UseReColour a1..a5 the five colour bytes | UsePreColoured | SetFont a1 font or -1
//   GetFont (a0 parameters): the font's label
//   tear-down a0 kind (0 characters, 1 lines, 2 points), a1 capacity word, a2 size word, a3 bytes of the block (0: no block)
// A parameters reference is 0 to 2 for the stand-alone objects, 100 and 101 for a formatter's own, 200 and 201 for a render's own.
enum Op {
    kOpFormatterCtor = 0, kOpFormatterDtor, kOpSetVertical, kOpSetHorizontal, kOpGetArray, kOpGetArraySize, kOpIdxFromPosition,
    kOpCharType, kOpCreateInfoList, kOpLayout, kOpLayoutLines, kOpDrawText, kOpEmptyString, kOpDraw, kOpMeasure,
    kOpRenderCtor, kOpRenderDtor, kOpRenderInitialise, kOpRenderDrawChar,
    kOpParamsCtor, kOpParamsDtor, kOpUseReColour, kOpUsePreColoured, kOpSetFont,
    kOpTearDown, kOpGetFont,
};

static TbRect RectOf(const int64_t* a) {
    return {static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4])};
}

int64_t tpw_tf_op(int32_t op, const int64_t* a, int64_t* out) {
    int64_t result = 0;
    for (int i = 0; i < 8; ++i) out[i] = 0;
    const int32_t n = static_cast<int32_t>(a[0]);
    switch (op) {
        case kOpFormatterCtor: {
            void* storage = tpw_tf_storage(0, n, static_cast<int32_t>(a[1]));
            tpw_cl_label(kLabelFormatter + n, storage);
            new (storage) TbTextFormatter;
            tpw_cl_label(kLabelFormatterParams + n, &gFormatter[n]->mRender.mParams);
            break;
        }
        case kOpFormatterDtor:
            if (a[1] > 0) {
                delete gFormatter[n];   // the deleting form: the destructor, then the block goes back
            } else {
                gFormatter[n]->~TbTextFormatter();   // the block stays (and is forgotten): its vectors' blocks are gone
            }
            gFormatter[n] = nullptr;
            break;
        case kOpSetVertical: result = gFormatter[n]->SetVerticalJustification(static_cast<TbTextFormatter::eJustification>(a[1])); break;
        case kOpSetHorizontal: result = gFormatter[n]->SetHorizontalJustification(static_cast<TbTextFormatter::eJustification>(a[1])); break;
        case kOpGetArray: result = lab(gFormatter[n]->GetCharPositionArray()); break;
        case kOpGetArraySize: result = gFormatter[n]->GetCharPositionArraySize(); break;
        case kOpIdxFromPosition:
            result = gFormatter[n]->GetCharIdxFromPosition(TbPoint{static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])});
            break;
        case kOpCharType: result = gFormatter[n]->GetCharType(static_cast<char16_t>(a[1])); break;
        case kOpCreateInfoList: result = gFormatter[n]->CreateInfoList(gTexts[a[1]].data(), *ParamsOf(static_cast<int>(a[2]))); break;
        case kOpLayout: result = gFormatter[n]->Layout(RectOf(a)); break;
        case kOpLayoutLines: {
            TbTextFormatter* f = gFormatter[n];
            result = f->LayoutLines(f->mChars.mData + a[1], f->mChars.mData + a[2]);
            break;
        }
        case kOpDrawText: {
            TbRect bounds = {-1, -2, -3, -4};
            result = gFormatter[n]->DrawText(*ParamsOf(static_cast<int>(a[1])), a[2] != 0 ? &bounds : nullptr, a[3] != 0);
            out[0] = bounds.left;
            out[1] = bounds.top;
            out[2] = bounds.right;
            out[3] = bounds.bottom;
            break;
        }
        case kOpEmptyString: result = gFormatter[n]->EmptyStringProcess(RectOf(a)); break;
        case kOpDraw:
        case kOpMeasure: {
            TbRect drawn = {-1, -2, -3, -4};
            const uint8_t* text = gTexts[a[5]].data();
            TbFontRenderParams& params = *ParamsOf(static_cast<int>(a[6]));
            TbRect* outRect = a[7] != 0 ? &drawn : nullptr;
            const TbRect rect = RectOf(a);
            result = op == kOpDraw ? gFormatter[n]->Draw(rect, text, params, outRect) : gFormatter[n]->Measure(rect, text, params, outRect);
            out[0] = drawn.left;
            out[1] = drawn.top;
            out[2] = drawn.right;
            out[3] = drawn.bottom;
            break;
        }
        case kOpRenderCtor: {
            void* storage = tpw_tf_storage(2, n, static_cast<int32_t>(a[1]));
            tpw_cl_label(kLabelRender + n, storage);
            new (storage) TbFontRender;
            tpw_cl_label(kLabelRenderParams + n, &gRender[n]->mParams);
            break;
        }
        case kOpRenderDtor:
            if (a[1] > 0) {
                delete gRender[n];
                gRender[n] = nullptr;
            } else {
                gRender[n]->~TbFontRender();
            }
            break;
        case kOpRenderInitialise: result = gRender[n]->Initialise(*ParamsOf(static_cast<int>(a[1]))); break;
        case kOpRenderDrawChar: {
            const TbPoint at = {static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])};
            TbRect drawn = {-1, -2, -3, -4};
            result = gRender[n]->DrawChar(at, static_cast<uint16_t>(a[3]), drawn);
            out[0] = drawn.left;
            out[1] = drawn.top;
            out[2] = drawn.right;
            out[3] = drawn.bottom;
            break;
        }
        case kOpParamsCtor: {
            void* storage = tpw_tf_storage(1, n, static_cast<int32_t>(a[1]));
            tpw_cl_label(kLabelParams + n, storage);
            new (storage) TbFontRenderParams;
            break;
        }
        case kOpParamsDtor:
            if (a[1] > 0) {
                delete gParams[n];
                gParams[n] = nullptr;
            } else {
                gParams[n]->~TbFontRenderParams();
            }
            break;
        case kOpUseReColour: {
            alignas(TbColour) uint8_t bytes[sizeof(TbColour)];
            static_assert(sizeof(TbColour) == 5, "a colour is five bytes");
            for (int i = 0; i < 5; ++i) bytes[i] = static_cast<uint8_t>(a[1 + i]);
            result = gParams[n]->UseReColour(*reinterpret_cast<const TbColour*>(bytes));
            break;
        }
        case kOpUsePreColoured: result = gParams[n]->UsePreColoured(); break;
        case kOpSetFont: result = gParams[n]->SetFont(a[1] < 0 ? nullptr : &gFonts[a[1]]); break;
        case kOpGetFont: result = lab(gParams[n]->GetFont()); break;   // (unit w44; the answer is the font's label)
        case kOpTearDown: {
            void* block = a[3] != 0 ? std::malloc(static_cast<std::size_t>(a[3])) : nullptr;
            if (block != nullptr) newlab(block);
            if (n == 0) {
                MslVector<CCharacterInfo> v = {static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]), static_cast<CCharacterInfo*>(block)};
                v.tear_down();
            } else if (n == 1) {
                MslVector<CLineInfo> v = {static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]), static_cast<CLineInfo*>(block)};
                v.tear_down();
            } else {
                MslVector<TbPoint> v = {static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2]), static_cast<TbPoint*>(block)};
                v.tear_down();
            }
            break;
        }
    }
    return result;
}

}  // extern "C"
