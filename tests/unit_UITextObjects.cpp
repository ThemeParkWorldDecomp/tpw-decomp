// Test library for the text draw objects (libtpw_unit_UITextObjects.dylib; see tests/test_UITextObjects.py): src/ui_DrawObjectText.cpp
// (InterfaceTextObjectSprite, InterfaceTitleTextObject and InterfaceCursorPosTextObjectSprite, the ten functions of unit w35c), under it the
// real sprite draw object (src/ui_DrawObjectSprite.cpp), the real rectangles (src/UI_Base.cpp) and the real font table classes
// (src/InterfaceFont.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: the draw object base class, the engine's sprite surfaces (the
// game data, the surface and the sprite list) and the map, the text formatter, the font parameters, the render area helpers, the Hebrew
// swap, the string converter, the drawing surface and the palette. The formatter's Draw writes a mark into the surface it is drawing into,
// so that the order of the clearing and the drawing shows in the pixels.
//
// The Python module drives this library through extern "C" operations on a small world: SLOTS text objects of the three kinds made on the
// heap, a pixel buffer, six fonts, six texts and a table of character positions. The dumps write the state in the order of the Python
// module's tables.
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceFontWrapper.h"
#include "LbPalette.h"
#include "Text.h"
#include "calllog.h"
#include "ui_DrawObject.h"

using namespace calllog;

// InterfaceFont::GetHeight passes a wrapper on to its own (src/InterfaceFont.cpp), which lives in a file this unit does not link;
// no case here asks a wrapper for its height.
int32_t InterfaceFontWrapper::GetHeight() { return 0; }

namespace {
constexpr int kSlots = 6;
constexpr int kFonts = 6;
constexpr int kTexts = 6;
constexpr int kTokens = 3;
constexpr int kPositionRoom = 8;
constexpr int kTextRoom = 200;
constexpr int kPixelBytes = 20000;
// labels (the Python module's LAB_*): the objects 30.., and for each its game data 40.., surface 50.., list 60.. (and its link base: the
// same label), formatter 70.. and instance array 80..; then the pixels, the map, the font parameters, the palette, the converted text and
// the query rectangle, the fonts 100.., the texts 110.. and the data tokens 120..
constexpr int kObjectLabel = 30, kGameDataLabel = 40, kSurfaceLabel = 50, kListLabel = 60, kFormatterLabel = 70, kInstancesLabel = 80,
              kPixelsLabel = 90, kMapLabel = 91, kParamsLabel = 92, kPaletteLabel = 93, kConvertedLabel = 94, kQueryLabel = 95,
              kFontLabel = 100, kTextLabel = 110, kTokenLabel = 120;
constexpr int kKindNone = 0, kKindText = 1, kKindTitle = 2, kKindCursor = 3;
}  // namespace

// ---- the globals the unit does not define (the screen's size is UI_Base.cpp's)
CMapWho* gei_MapWho;
struct TbPalette {
    char mOpaque[4];
};
TbPalette _lbGlobalPalette;
CText gText;

namespace {
alignas(16) uint8_t gPixels[kPixelBytes];
char gMapStorage[16];
alignas(16) char gInstanceArrays[kSlots][16];   // what each sprite list's Init hands out (room for the one entry)
char gTokens[kTokens];
char gFontTokens[kFonts];
char16_t gTexts[kTexts][kTextRoom];
char gConverted[kTextRoom];                    // what the fake converter hands out
TbPoint gPositions[kPositionRoom];
TbRenderArea* gCurrent;                         // the area LbDraw_SetCurrent was last given
int32_t gDrawCount;                             // how many times the formatter was told to draw (for the marks)

InterfaceTextObjectSprite* gObjects[kSlots];
int gKinds[kSlots];

// Objects destroyed in place (a destructor called without a delete): their storage is freed at the next reset, unrecorded.
void* gDead[16];
int gDeadCount;
void Bury(void* block) {
    if (gDeadCount < 16) gDead[gDeadCount++] = block;
}

InterfaceTextObjectSprite* Object(int64_t slot) { return slot >= 0 && slot < kSlots ? gObjects[slot] : nullptr; }

// The slot of the object whose list, game data or surface this is (by its label), or -1.
int64_t SlotOf(const void* object, int base) {
    const int64_t label = lab(object);
    return label >= base && label < base + kSlots ? label - base : -1;
}
}  // namespace

// ---- the draw object base class: the fake constructor sets the words the real one sets (and the depth, which it leaves) to the data and
// zeros, and records; the other members are only reached through the vtables
InterfaceDrawObject::InterfaceDrawObject(void* data) {
    ev("DrawObject.ctor", {lab(this), lab(data)});
    mData = data;
    mShown = 0;
    mFrameBase = 0;
    mFrame = 0;
    mFrameCount = 0;
    mInLibrary = 0;
    mResizable = 0;
    mRect.Set(0, 0, 0, 0);
    mRect2.Set(0, 0, 0, 0);
    mF28 = 0;
    mF2A = 0;
    mF2C = 0;
    mF2E = 0;
    mF30 = 0;
    mF32 = 0;
    mDepth = 0;
}
InterfaceDrawObject::~InterfaceDrawObject() { ev("DrawObject.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// ---- the engine's sprite surfaces and the map
SSpriteGameData::SSpriteGameData() { ev("GameData.ctor", {lab(this)}); }
void SSpriteGameData::SetUsedPosition(int32_t x, int32_t y) { ev("SetUsedPosition", {lab(this), x, y}); }
void SSpriteGameData::SetUsedSize(int32_t width, int32_t height) { ev("SetUsedSize", {lab(this), width, height}); }

// (zeroes the words, as the real constructor does)
SSpriteEngineData::SSpriteEngineData() {
    ev("Surface.ctor", {lab(this)});
    mTexture = nullptr;
    mWords.mPixels = nullptr;
    mWords.mAspect = 0.0f;
    mWords.mWidth = 0;
    mWords.mHeight = 0;
}
SSpriteEngineData::~SSpriteEngineData() { ev("Surface.dtor", {lab(this)}); }
// The surface of the size asked for, with pixels (the buffer holds 4 bytes a pixel for surfaces up to 5,000 pixels) unless the queue says
// the allocation failed or the size is out of the buffer's reach.
int32_t SSpriteEngineData::Init(int32_t width, int32_t height) {
    ev("Surface.Init", {lab(this), width, height});
    mWords.mWidth = static_cast<uint16_t>(width);
    mWords.mHeight = static_cast<uint16_t>(height);
    const int64_t bytes = static_cast<int64_t>(static_cast<uint16_t>(width)) * static_cast<uint16_t>(height) * 4;
    const bool made = ret("Surface.Init", 1) != 0 && bytes <= kPixelBytes;
    mWords.mPixels = made ? gPixels : nullptr;
    return made ? 1 : 0;
}
int32_t SSpriteEngineData::RefreshPartialTexture() { ev("RefreshPartialTexture", {lab(this)}); return 1; }   // int32_t since w33b

CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() { ev("List.ctor", {lab(this)}); }
CSpriteList::~CSpriteList() { ev("List.dtor", {lab(this)}); }
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
SSpriteGameData** CSpriteList::Init(int32_t count) {
    ev("List.Init", {lab(this), count});
    const int64_t slot = SlotOf(this, kListLabel);
    return slot >= 0 ? reinterpret_cast<SSpriteGameData**>(gInstanceArrays[slot]) : nullptr;
}

namespace {
// The label of the sprite list a link base belongs to (the link base is the list as far as the log is concerned; on the Mac it is a
// separate part of the list's object, the PowerPC side labels its address with the list's label).
int64_t LinkLabel(CMapLinkBase* link) {
    const CSpriteList* list = dynamic_cast<CSpriteList*>(link);
    return list != nullptr ? lab(list) : lab(link);
}
}  // namespace
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("MapWho.Link", {lab(this), LinkLabel(link), list});
    return 1;
}
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("MapWho.Unlink", {lab(this), LinkLabel(link), list});
    return 1;
}

// ---- the render area, its helpers and the drawing surface
// (the real constructor zeroes the words from 0x10 on)
TbRenderArea::TbRenderArea() {
    ev("RenderArea.ctor");
    mDepth = 0;
    mHeight = 0;
    mData = nullptr;
    mWidth = 0;
    mF20 = 0;
    mStride = 0;
}

void AdjustJustificationForHebrew(int32_t& justification) {
    ev("AdjustJustification", {justification});
    if (ret("Hebrew", 0) != 0) {
        if (justification == 0) justification = 2;
        else if (justification == 2) justification = 0;
    }
}

// The area over the pixels: the format's masks zero, the depth from the queue (the screen's), the picture's size and the row in bytes.
void MakeRenderArea(uint8_t* pixels, TbRenderArea* area, int32_t width, int32_t height) {
    ev("MakeRenderArea", {lab(pixels), width, height});
    if (area == nullptr) return;
    const int32_t depth = static_cast<int32_t>(ret("Depth", 16));
    std::memset(area->mF00, 0, sizeof area->mF00);
    area->mDepth = depth;
    area->mPalettised = 0;
    area->mData = pixels;
    area->mWidth = width;
    area->mPictureHeight = height;
    area->mStride = static_cast<int32_t>(static_cast<uint32_t>(depth) >> 3) * width;
}

void LbDraw_SetCurrent(TbRenderArea* area) {
    ev("SetCurrent", {area->mDepth, area->mWidth, area->mPictureHeight, area->mStride, lab(area->mData)});
    gCurrent = area;
}

uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    ev("FindColour", {lab(palette), red, green, blue});
    return static_cast<uint8_t>(ret("FindColour", 0x5a));
}

namespace {
// The text as the log records it (calllog's narrow, into a buffer of the unit's own: a std::string longer than its inline room would
// allocate, and the test compares the heap traffic).
const char* Narrowed(const char16_t* text) {
    static char buffer[160];
    if (text == nullptr) return "(null)";
    std::size_t n = 0;
    for (; text[n] != 0 && n < 159; ++n) buffer[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    buffer[n] = 0;
    return buffer;
}
}  // namespace

// The text in the language's code page: the characters under 0x80 kept, any other a '?'.
unsigned char* UIMain_ConvertString(char16_t* text) {
    ev("ConvertString", {lab(text)}, {Narrowed(text)});
    std::size_t n = 0;
    for (; text != nullptr && text[n] != 0 && n + 1 < sizeof gConverted; ++n) gConverted[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    gConverted[n] = 0;
    return reinterpret_cast<unsigned char*>(gConverted);
}

// The unit's colours of a TbColour made on the stack: the fields are all set by the code (the Mac's constructor is inline).
TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}

// ---- the font parameters, the font table and the resource manager of the language
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbError TbFontRenderParams::UseAlphaWriteMode() {
    ev("UseAlphaWriteMode", {lab(this)});
    return TbError(0);
}
TbError TbFontRenderParams::SetFont(TbIFont* font) {
    ev("SetFont", {lab(this), lab(font)});
    return TbError(0);
}
TbError TbFontRenderParams::UseReColour(const TbColour& colour) {
    ev("UseReColour", {lab(this), colour.mAlpha, colour.mRed, colour.mGreen, colour.mBlue, colour.mIndex});
    return TbError(0);
}
TbError TbFontRenderParams::UsePreColoured() {
    ev("UsePreColoured", {lab(this)});
    return TbError(0);
}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

// ---- the text formatter. Draw and Measure record the box and the rectangle they are given, and give back a rectangle of the queue's
// (four values a call; the box moved by a few pixels without one). Draw marks the surface it draws into.
// The font renderer the real formatter (w35a's header, since the merge of 2026-10-12) owns by value: fakes that do nothing.
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() {
    ev("Formatter.ctor", {lab(this)});
    mMeasurePositions = 0;
    mCaret.x = 0;
    mCaret.y = 0;
}
TbTextFormatter::~TbTextFormatter() { ev("Formatter.dtor", {lab(this)}); }
TbError TbTextFormatter::SetVerticalJustification(eJustification where) {
    ev("Formatter.SetVertical", {lab(this), where});
    return TbError(0);
}
TbError TbTextFormatter::SetHorizontalJustification(eJustification where) {
    ev("Formatter.SetHorizontal", {lab(this), where});
    return TbError(0);
}
const TbPoint* TbTextFormatter::GetCharPositionArray() const {
    ev("Formatter.GetCharPositionArray", {lab(this)});
    return gPositions;
}
uint32_t TbTextFormatter::GetCharPositionArraySize() const {
    ev("Formatter.GetCharPositionArraySize", {lab(this)});
    return static_cast<uint32_t>(ret("PositionCount", 0));
}

namespace {
TbError Lay(const char* name, TbTextFormatter* formatter, const TbRect& box, const unsigned char* text, TbFontRenderParams& params,
            TbRect* bounds) {
    ev(name, {lab(formatter), box.left, box.top, box.right, box.bottom, lab(text), lab(&params), bounds != nullptr ? 1 : 0,
              bounds != nullptr ? bounds->left : 0, bounds != nullptr ? bounds->top : 0, bounds != nullptr ? bounds->right : 0,
              bounds != nullptr ? bounds->bottom : 0},
       {reinterpret_cast<const char*>(text)});
    if (bounds != nullptr) {
        bounds->left = static_cast<int32_t>(ret("Bounds", box.left + 3));
        bounds->top = static_cast<int32_t>(ret("Bounds", box.top + 5));
        bounds->right = static_cast<int32_t>(ret("Bounds", box.right - 7));
        bounds->bottom = static_cast<int32_t>(ret("Bounds", box.bottom - 11));
    }
    return TbError(static_cast<int32_t>(ret("Lay", 0)));
}
}  // namespace

TbError TbTextFormatter::Draw(const TbRect& box, const unsigned char* text, TbFontRenderParams& params, TbRect* bounds) {
    const TbError result = Lay("Formatter.Draw", this, box, text, params, bounds);
    // the mark: four bytes of the box, in the surface the last LbDraw_SetCurrent named, at a place that moves with each draw
    if (gCurrent != nullptr && gCurrent->mData != nullptr) {
        uint8_t* place = gCurrent->mData + 4 * (gDrawCount & 31) + 8;
        place[0] = static_cast<uint8_t>(box.left);
        place[1] = static_cast<uint8_t>(box.top);
        place[2] = static_cast<uint8_t>(box.right);
        place[3] = static_cast<uint8_t>(box.bottom);
    }
    ++gDrawCount;
    return result;
}
TbError TbTextFormatter::Measure(const TbRect& box, const unsigned char* text, TbFontRenderParams& params, TbRect* bounds) {
    return Lay("Formatter.Measure", this, box, text, params, bounds);
}

namespace {
// The callbacks the sprite object calls around the drawing: each records the area and puts a mark in the surface (the pre-draw one
// before the clearing, which a zero answer leaves to happen).
void RecordArea(const char* name, TbRenderArea& area) {
    ev(name, {area.mDepth, lab(area.mData), area.mWidth, area.mPictureHeight, area.mStride});
}
int32_t PreDraw(TbRenderArea& area) {
    RecordArea("PreDraw", area);
    if (area.mData != nullptr) area.mData[200] = 0xc3;
    return static_cast<int32_t>(ret("PreDraw", 0));
}
int32_t PostDraw(TbRenderArea& area) {
    RecordArea("PostDraw", area);
    if (area.mData != nullptr) area.mData[201] = 0x3c;
    return 0;
}
}  // namespace

// ---- the dumps and the operations
namespace {
int64_t Heap[4];

int64_t L(const void* object) { return lab(object); }
void Put(int64_t*& out, int64_t value) { *out++ = value; }

bool Untouched(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}

uint32_t FloatBits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

std::size_t SizeOf(int kind) {
    return kind == kKindTitle ? sizeof(InterfaceTitleTextObject) : kind == kKindCursor ? sizeof(InterfaceCursorPosTextObjectSprite)
                                                                                      : sizeof(InterfaceTextObjectSprite);
}

void FreeObject(int slot) {
    if (gObjects[slot] != nullptr) ::operator delete(gObjects[slot]);
    gObjects[slot] = nullptr;
    gKinds[slot] = kKindNone;
}

// The records the staged style and change operations make for a Change.
DOTextStyle gStyle;
bool gStyleNull;
DOChange gChange;
bool gRectNull;
CoolRect gStagedRect;
}  // namespace

extern "C" {

// Resets the world for a case: no objects, the globals as the Mac's data has them (zero), the pixels untouched, the font table empty.
void tpw_uitx_reset() {
    for (int i = 0; i < gDeadCount; ++i) ::operator delete(gDead[i]);
    gDeadCount = 0;
    for (int i = 0; i < kSlots; ++i) FreeObject(i);
    gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
    gUIRealScreenWidth = 0;
    gUIRealScreenHeight = 0;
    gText.mResolutionGroup = 0;
    for (auto& font : gText.mFonts) font.mFont = nullptr;
    std::memset(gPixels, 0xa5, sizeof gPixels);
    std::memset(gPositions, 0, sizeof gPositions);
    std::memset(gConverted, 0, sizeof gConverted);
    gCurrent = nullptr;
    gDrawCount = 0;
    gUIQueryRect.Set(0, 0, 0, 0);
    gStyleNull = false;
    gRectNull = false;
    std::memset(static_cast<void*>(&gStyle), 0xa5, sizeof gStyle);
    gStyle.mText = nullptr;
    std::memset(static_cast<void*>(&gChange), 0xa5, sizeof gChange);
    gStagedRect.Set(0, 0, 0, 0);
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (the objects and their parts are named when made).
void tpw_uitx_labels() {
    for (int i = 0; i < kSlots; ++i) tpw_cl_label(kInstancesLabel + i, gInstanceArrays[i]);
    tpw_cl_label(kPixelsLabel, gPixels);
    tpw_cl_label(kMapLabel, gMapStorage);
    tpw_cl_label(kParamsLabel, &gText.mRenderParams);
    tpw_cl_label(kPaletteLabel, &_lbGlobalPalette);
    tpw_cl_label(kConvertedLabel, gConverted);
    tpw_cl_label(kQueryLabel, &gUIQueryRect);
    for (int i = 0; i < kFonts; ++i) tpw_cl_label(kFontLabel + i, &gFontTokens[i]);
    for (int i = 0; i < kTexts; ++i) tpw_cl_label(kTextLabel + i, gTexts[i]);
    for (int i = 0; i < kTokens; ++i) tpw_cl_label(kTokenLabel + i, &gTokens[i]);
}

// A text of the world, UTF-16 (the PowerPC side builds the same from the same list).
void tpw_uitx_text(int32_t index, const uint16_t* chars, int32_t count) {
    if (index < 0 || index >= kTexts || count < 0 || count >= kTextRoom) return;
    for (int32_t i = 0; i < count; ++i) gTexts[index][i] = static_cast<char16_t>(chars[i]);
    gTexts[index][count] = 0;
}

const uint8_t* tpw_uitx_pixels() { return gPixels; }

// The words of an object the dump reads, as one number each (see tpw_uitx_dump_object).
void tpw_uitx_dump_object(int32_t slot, int64_t* out) {
    const InterfaceTextObjectSprite* s = gObjects[slot];
    Put(out, gKinds[slot]);
    Put(out, L(s->mData));
    Put(out, s->mShown);
    Put(out, s->mFrameBase);
    Put(out, s->mFrame);
    Put(out, s->mFrameCount);
    Put(out, s->mInLibrary);
    Put(out, s->mResizable);
    for (const CoolRect* rect : {&s->mRect, &s->mRect2}) {
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    }
    Put(out, s->mF28);
    Put(out, s->mF2A);
    Put(out, s->mF2C);
    Put(out, s->mF2E);
    Put(out, s->mF30);
    Put(out, s->mF32);
    Put(out, s->mDepth);
    Put(out, s->mPixelWidth);
    Put(out, s->mPixelHeight);
    Put(out, Untouched(s->mF40, sizeof s->mF40));
    Put(out, s->mCreated);
    for (int i = 0; i < 6; ++i) Put(out, s->mF46[i]);
    Put(out, s->mPreDraw == nullptr ? 0 : 1);
    Put(out, s->mPostDraw == nullptr && s->mF50 == 0 ? 0 : 1);   // (the Mac's word at 0x50)
    // the game data
    Put(out, static_cast<int32_t>(s->mSprite.mF10));
    Put(out, FloatBits(s->mSprite.mPosition.x));
    Put(out, FloatBits(s->mSprite.mPosition.y));
    Put(out, FloatBits(s->mSprite.mPosition.z));
    Put(out, L(s->mSprite.mSprite));
    Put(out, FloatBits(s->mSprite.mF2C));
    Put(out, static_cast<int32_t>(s->mSprite.mF24));
    // the instance array and the list
    Put(out, L(s->mInstances));
    Put(out, s->mInstances == nullptr ? 0 : L(s->mInstances[0]));
    Put(out, s->mList.mCount);
    // the surface
    Put(out, L(s->mEngineData.mTexture));
    Put(out, L(s->mEngineData.mWords.mPixels));
    Put(out, s->mEngineData.mWords.mWidth);
    Put(out, s->mEngineData.mWords.mHeight);
    // the text style
    Put(out, s->mJustify);
    Put(out, s->mVJustify);
    Put(out, s->mWrap);
    Put(out, s->mF0DF);
    Put(out, s->mFontId);
    Put(out, s->mColourMode);
    Put(out, s->mOffsetX);
    Put(out, s->mOffsetY);
    // the formatter's words the unit touches
    Put(out, s->mFormatter.mMeasurePositions);
    Put(out, s->mFormatter.mCaret.x);
    Put(out, s->mFormatter.mCaret.y);
}

// The globals: the screen's size, the resolution group, the map pointer's label, the query rectangle, the number of draws and the draw
// area the formatter was last given (whether it is set).
void tpw_uitx_dump_globals(int64_t* out) {
    Put(out, gUIRealScreenWidth);
    Put(out, gUIRealScreenHeight);
    Put(out, static_cast<int32_t>(gText.mResolutionGroup));
    Put(out, L(gei_MapWho));
    Put(out, gUIQueryRect.mLeft);
    Put(out, gUIQueryRect.mTop);
    Put(out, gUIQueryRect.mRight);
    Put(out, gUIQueryRect.mBottom);
    Put(out, gDrawCount);
}

void tpw_uitx_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) Put(out, Heap[i]);
}

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the Python module.
void tpw_uitx_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceTextObjectSprite* object = Object(a[0]);
    const int64_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    gCurrent = nullptr;
    switch (op) {
    case 1: {   // an object: slot, kind, data token (-1 NULL)
        FreeObject(static_cast<int>(a[0]));
        const int kind = static_cast<int>(a[1]);
        void* block = ::operator new(SizeOf(kind));
        std::memset(block, 0xa5, SizeOf(kind));
        const int64_t made[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};   // (the block above is the test's own)
        const int32_t s = static_cast<int32_t>(a[0]);
        auto* p = static_cast<InterfaceTextObjectSprite*>(block);
        // (the parts are named before the constructor makes them; the casts give the addresses the members will have)
        tpw_cl_label(kObjectLabel + s, block);
        tpw_cl_label(kGameDataLabel + s, &p->mSprite);
        tpw_cl_label(kSurfaceLabel + s, &p->mEngineData);
        tpw_cl_label(kListLabel + s, &p->mList);
        tpw_cl_label(kFormatterLabel + s, &p->mFormatter);
        void* data = a[2] >= 0 ? static_cast<void*>(&gTokens[a[2]]) : nullptr;
        gKinds[a[0]] = kind;
        if (kind == kKindText) gObjects[a[0]] = new (block) InterfaceTextObjectSprite(data);
        else if (kind == kKindTitle) gObjects[a[0]] = new (block) InterfaceTitleTextObject(data);
        else gObjects[a[0]] = new (block) InterfaceCursorPosTextObjectSprite(data);
        out[0] = L(gObjects[a[0]]);
        for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - made[i];
        return;
    }
    case 2:   // the text style: slot, justify, vjustify, wrap, the byte at 0xdf, font id, colour mode, offset x, offset y, the four colour bytes
        object->mJustify = static_cast<int8_t>(a[1]);
        object->mVJustify = static_cast<int8_t>(a[2]);
        object->mWrap = static_cast<uint8_t>(a[3]);
        object->mF0DF = static_cast<uint8_t>(a[4]);
        object->mFontId = static_cast<int8_t>(a[5]);
        object->mColourMode = static_cast<uint8_t>(a[6]);
        object->mOffsetX = static_cast<int16_t>(a[7]);
        object->mOffsetY = static_cast<int16_t>(a[8]);
        for (int i = 0; i < 4; ++i) object->mF46[2 + i] = static_cast<uint8_t>(a[9 + i]);
        return;
    case 3:   // the sprite's words: slot, created, pixel width, pixel height, shown, rectangle (4), depth, word 0x2c, surface width, surface height, pixels (0 none, 1 the buffer)
        object->mCreated = static_cast<int16_t>(a[1]);
        object->mPixelWidth = static_cast<int16_t>(a[2]);
        object->mPixelHeight = static_cast<int16_t>(a[3]);
        object->mShown = static_cast<int16_t>(a[4]);
        object->mRect.mLeft = static_cast<int16_t>(a[5]);
        object->mRect.mTop = static_cast<int16_t>(a[6]);
        object->mRect.mRight = static_cast<int16_t>(a[7]);
        object->mRect.mBottom = static_cast<int16_t>(a[8]);
        object->mDepth = static_cast<int16_t>(a[9]);
        object->mF2C = static_cast<int16_t>(a[10]);
        object->mEngineData.mWords.mWidth = static_cast<uint16_t>(a[11]);
        object->mEngineData.mWords.mHeight = static_cast<uint16_t>(a[12]);
        object->mEngineData.mWords.mPixels = a[13] == 0 ? nullptr : static_cast<void*>(gPixels);
        return;
    case 4:   // the second rectangle: slot, four
        object->mRect2.mLeft = static_cast<int16_t>(a[1]);
        object->mRect2.mTop = static_cast<int16_t>(a[2]);
        object->mRect2.mRight = static_cast<int16_t>(a[3]);
        object->mRect2.mBottom = static_cast<int16_t>(a[4]);
        return;
    case 5:   // a global: which (1 screen width, 2 screen height, 3 resolution group), value
        switch (a[0]) {
        case 1: gUIRealScreenWidth = static_cast<int16_t>(a[1]); break;
        case 2: gUIRealScreenHeight = static_cast<int16_t>(a[1]); break;
        case 3: gText.mResolutionGroup = static_cast<uint32_t>(a[1]); break;
        }
        return;
    case 6:   // a group of the font table: group, then the font token (-1 none) of each of the 13 fonts
        for (int i = 0; i < 13; ++i) gText.mFonts[a[0] * 13 + i].mFont = a[1 + i] >= 0 ? reinterpret_cast<TbIFont*>(&gFontTokens[a[1 + i]]) : nullptr;
        return;
    case 7:   // the formatter's words: slot, the measure-draws byte, the pen position
        object->mFormatter.mMeasurePositions = static_cast<uint8_t>(a[1]);
        object->mFormatter.mCaret.x = static_cast<int32_t>(a[2]);
        object->mFormatter.mCaret.y = static_cast<int32_t>(a[3]);
        return;
    case 8:   // the callbacks: slot, pre-draw (0 none, 1 the fake), post-draw
        object->mPreDraw = a[1] != 0 ? PreDraw : nullptr;
        object->mPostDraw = a[2] != 0 ? PostDraw : nullptr;
        return;
    case 9:   // the character positions: the count, then the pairs
        for (int i = 0; i < kPositionRoom; ++i) {
            gPositions[i].x = i < a[0] ? static_cast<int32_t>(a[1 + 2 * i]) : 0;
            gPositions[i].y = i < a[0] ? static_cast<int32_t>(a[2 + 2 * i]) : 0;
        }
        return;
    case 10:   // the staged text style: the justify, vjustify, wrap, byte 7, font, colour mode, offset x, y, four colour bytes, the text token (-1 none), 1 for no style at all
        std::memset(static_cast<void*>(&gStyle), 0xa5, sizeof gStyle);
        gStyle.mF04 = static_cast<uint8_t>(a[0]);
        gStyle.mF05 = static_cast<uint8_t>(a[1]);
        gStyle.mF06[0] = static_cast<uint8_t>(a[2]);
        gStyle.mF06[1] = static_cast<uint8_t>(a[3]);
        gStyle.mF06[2] = static_cast<uint8_t>(a[4]);
        gStyle.mF09 = static_cast<uint8_t>(a[5]);
        {
            const int16_t x = static_cast<int16_t>(a[6]);
            const int16_t y = static_cast<int16_t>(a[7]);
            std::memcpy(&gStyle.mF0A[0], &x, sizeof x);
            std::memcpy(&gStyle.mF0A[2], &y, sizeof y);
        }
        gStyle.mColour[0] = static_cast<uint8_t>(a[8]);
        gStyle.mColour[1] = static_cast<uint8_t>(a[9]);
        gStyle.mColour[2] = static_cast<uint8_t>(a[10]);
        gStyle.mF13 = static_cast<uint8_t>(a[11]);
        gStyle.mText = a[12] >= 0 ? gTexts[a[12]] : nullptr;
        gStyleNull = a[13] != 0;
        return;
    case 11:   // the staged change record: depth, word 8, the rectangle (4), 1 for no rectangle
        std::memset(static_cast<void*>(&gChange), 0xa5, sizeof gChange);
        gChange.mDepth = static_cast<int16_t>(a[0]);
        gChange.mF08 = static_cast<int16_t>(a[1]);
        gStagedRect.Set(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]));
        gRectNull = a[6] != 0;
        return;
    case 12:   // the query rectangle: four
        gUIQueryRect.Set(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]));
        return;
    default:
        break;
    }
    // ---- the functions under test (op, slot in a[0])
    switch (op) {
    case 20:   // UpdateTextures through the vtable: slot, text token (-1 NULL)
        object->UpdateTextures(a[1] >= 0 ? gTexts[a[1]] : nullptr);
        break;
    case 21: {   // Change: slot, flags, 1 for no record
        gChange.mText = gStyleNull ? nullptr : &gStyle;
        gChange.mRect = gRectNull ? nullptr : &gStagedRect;
        object->Change(static_cast<uint32_t>(a[1]), a[2] != 0 ? nullptr : &gChange);
        break;
    }
    case 22: {   // Query: slot, what, value kind (0 a number, 1 a text token, -1 NULL), value. out: the answer (the query rectangle's label when it is its address)
        const intptr_t value = a[2] == 1 ? (a[3] >= 0 ? reinterpret_cast<intptr_t>(gTexts[a[3]]) : 0) : static_cast<intptr_t>(a[3]);
        const intptr_t answer = object->Query(static_cast<uint32_t>(a[1]), value);
        out[0] = answer == reinterpret_cast<intptr_t>(&gUIQueryRect) ? kQueryLabel : answer;
        break;
    }
    case 23:   // the destructor through the vtable: slot, flag (a positive short deletes the object; it is gone either way)
    case 24:   // (unit v5L49) a cursor object's destructor through textobj_callee::CursorDtor, the Mac-form one with the flag
        if (op == 24) {
            textobj_callee::CursorDtor(object, static_cast<int16_t>(a[1]));
            if (static_cast<int16_t>(a[1]) > 0) {
                tpw_cl_label(kGameDataLabel + static_cast<int32_t>(a[0]), nullptr);
                tpw_cl_label(kSurfaceLabel + static_cast<int32_t>(a[0]), nullptr);
                tpw_cl_label(kListLabel + static_cast<int32_t>(a[0]), nullptr);
                tpw_cl_label(kFormatterLabel + static_cast<int32_t>(a[0]), nullptr);
            } else {
                Bury(object);
            }
        } else if (static_cast<int16_t>(a[1]) > 0) {
            delete static_cast<InterfaceDrawObject*>(object);
            // (the parts' labels go too: a new object may be given the same memory)
            tpw_cl_label(kGameDataLabel + static_cast<int32_t>(a[0]), nullptr);
            tpw_cl_label(kSurfaceLabel + static_cast<int32_t>(a[0]), nullptr);
            tpw_cl_label(kListLabel + static_cast<int32_t>(a[0]), nullptr);
            tpw_cl_label(kFormatterLabel + static_cast<int32_t>(a[0]), nullptr);
        } else {
            static_cast<InterfaceDrawObject*>(object)->~InterfaceDrawObject();
            Bury(object);
        }
        gObjects[a[0]] = nullptr;
        gKinds[a[0]] = kKindNone;
        break;
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - heapBefore[i];
}

}  // extern "C"
