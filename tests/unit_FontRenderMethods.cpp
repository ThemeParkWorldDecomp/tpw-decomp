// Test library for the four 4-bit font render method objects and the one-colour glyph writers (libtpw_unit_FontRenderMethods.dylib;
// see tests/test_FontRenderMethods.py, unit w45b): src/ltms/TbOneColour4BitFontRenderMethod.cpp, src/ltms/TbOneColourNoBlend4BitFontRenderMethod.cpp, src/ltms/TbTextured4BitFontRenderMethod.cpp, src/ltms/TbTexturedNoBlend4BitFontRenderMethod.cpp (the constructors and destructors, SetWriteZero, the
// objects and the table gFontRenderMethods), src/ltms/TbOneColour4BitFontRenderMethod.cpp and src/ltms/TbOneColourNoBlend4BitFontRenderMethod.cpp (the
// twelve writers), linked with the render method's core of unit w45a (src/ltms/StaticUnicodeBuffer.cpp, src/ltms/TbFontRender.cpp: DrawChar), the real font
// (src/ltms/TbFontRenderParams.cpp, src/ltms/TbIRLE4bitFont.cpp) and the render parameters (src/ltms/TbFontRender.cpp, src/ltms/TbFontRenderParams.cpp), so that DrawChar can be
// driven through the table end to end.
//
// Fakes (each records the same event as the PowerPC side's stub, through calllog):
//   the textured classes' twelve writers (untranslated: they record the area, the point, the source rectangle and answer from a queue);
//   LbDraw_GetClipRect (answers a case-owned clip rectangle) and TbRect::Intersection (the formula of the listing, recording its
//   operands); the font's AddRef and Release (a real TbIRLE4bitFont over a font image the case supplies).
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
void* TbIResourceIndex::GetResourceItemHandle(uint32_t) { return nullptr; }
uint16_t TbIResourceIndex::GetResourceType(void*) { return 0; }
TbError TbIResourceIndex::GetPreOpenedResource(void*, void**) { return TbError(-1); }

// The render area's default constructor (the parameters' constructor makes one; the tests never run it).
TbRenderArea::TbRenderArea() {
    std::memset(this, 0, sizeof(*this));
}

TbRenderArea* _lbpCurrentRA = nullptr;

namespace {

TbRect gClip = {0, 0, 0, 0};

}  // namespace

// 0x1002b554: the clip rectangle the case supplies (the real one is tested in tests/test_LbDrawGetClipRect.py).
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

// ---- the textured classes' writers: untranslated, so each records what it is given and answers from the queue "Writer". The labels
// of the four objects are 50 + the class number (see Objects below).

namespace {

constexpr int kLabelObject = 50;

TbError TexturedWrite(const char* name, const Tb4BitFontRenderMethod* self, TbRenderArea& area, const C4BitFontRenderMethodParameters& p) {
    ev(name, {lab(self), lab(&area), p.mPoint->x, p.mPoint->y, p.mBytesPerRow, p.mSource->left, p.mSource->top, p.mSource->right,
              p.mSource->bottom, lab(p.mParams), p.mWidth});
    return TbError(static_cast<int32_t>(ret("Writer", 0)));
}

}  // namespace

TbError TbTextured4BitFontRenderMethod::DrawCharTo555(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.To555", this, a, p); }
TbError TbTextured4BitFontRenderMethod::DrawCharTo565(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.To565", this, a, p); }
TbError TbTextured4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.To4444", this, a, p); }
TbError TbTextured4BitFontRenderMethod::DrawCharToAny(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.ToAny", this, a, p); }
TbError TbTextured4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.To8888", this, a, p); }
TbError TbTextured4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("Textured.ToBGRA", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo555(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.To555", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo565(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.To565", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.To4444", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharToAny(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.ToAny", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.To8888", this, a, p); }
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& a, const C4BitFontRenderMethodParameters& p) { return TexturedWrite("TexturedNoBlend.ToBGRA", this, a, p); }

namespace {

// A real 4-bit font over a case-supplied image; only the reference calls are recorded.
class RecFont : public TbIRLE4bitFont {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
};

// A font for TbFontRender::Initialise: reference calls recorded, the type from the case.
class TypeFont : public TbIFont {
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
    uint32_t GetCaps() override { return 0; }
    uint32_t GetType() override {
        ev("GetType", {lab(this)});
        return type;
    }
    TbIMBToUnicode* GetMBToUnicode() override { return nullptr; }

    uint32_t type = 0;
};

alignas(TypeFont) unsigned char gTypeFontStorage[sizeof(TypeFont)];
alignas(TbFontRender) unsigned char gRenderStorage[sizeof(TbFontRender) + 32];

// The four objects by class number: 0 one colour (Mac 0x10011744), 1 one colour without blending (0x10011734), 2 textured
// (0x10011724), 3 textured without blending (0x10011714).
Tb4BitFontRenderMethod* Object(int cls) {
    switch (cls) {
        case 0: return &gOneColour4BitFontRenderMethod;
        case 1: return &gOneColourNoBlend4BitFontRenderMethod;
        case 2: return &gTextured4BitFontRenderMethod;
        default: return &gTexturedNoBlend4BitFontRenderMethod;
    }
}

// The class number of an object, by its dynamic type (-1 for none of the four).
int ClassOf(const TbFontRenderMethod* method) {
    if (dynamic_cast<const TbOneColour4BitFontRenderMethod*>(method) != nullptr) return 0;
    if (dynamic_cast<const TbOneColourNoBlend4BitFontRenderMethod*>(method) != nullptr) return 1;
    if (dynamic_cast<const TbTextured4BitFontRenderMethod*>(method) != nullptr) return 2;
    if (dynamic_cast<const TbTexturedNoBlend4BitFontRenderMethod*>(method) != nullptr) return 3;
    return -1;
}

void LabelObjects() {
    for (int cls = 0; cls < 4; ++cls) tpw_cl_label(kLabelObject + cls, Object(cls));
}

void ResetHeap() {
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
}

// Gives the memory of an object whose destructor was called by hand back without counting the operator delete.
void ReleaseQuietly(void* object) {
    const int32_t counted = tpw_cl_heap[2];
    ::operator delete(object);
    tpw_cl_heap[2] = counted;
}

// A new object of class `cls` on the heap (not counted).
TbFontRenderMethod* MakeOnHeap(int cls) {
    switch (cls) {
        case 0: return new TbOneColour4BitFontRenderMethod;
        case 1: return new TbOneColourNoBlend4BitFontRenderMethod;
        case 2: return new TbTextured4BitFontRenderMethod;
        default: return new TbTexturedNoBlend4BitFontRenderMethod;
    }
}

// The case's render area, parameters and font live here.
constexpr int kLabelFont = 10, kLabelArea = 20, kLabelParams = 30;
std::vector<uint8_t> gImage;
alignas(RecFont) unsigned char gFontStorage[sizeof(RecFont)];
alignas(TbRenderArea) unsigned char gAreaStorage[sizeof(TbRenderArea)];
alignas(TbFontRenderParams) unsigned char gParamsStorage[sizeof(TbFontRenderParams)];

// The colour of the parameters: the first five bytes of the slot at +0x44.
uint8_t* ColourOf(TbFontRenderParams* params) { return reinterpret_cast<uint8_t*>(&params->mState[16]); }

// A render area over `areaWords` = six words (the four masks, the depth, the palettised flag): the pixels start at `origin`.
TbRenderArea* MakeArea(const uint32_t* areaWords, int32_t stride, uint8_t* origin) {
    TbRenderArea* area = new (gAreaStorage) TbRenderArea;
    std::memcpy(area->mF00, areaWords, 16);
    area->mDepth = static_cast<int32_t>(areaWords[4]);
    area->mPalettised = static_cast<int32_t>(areaWords[5]);
    area->mData = origin;
    area->mWidth = 0x7777;
    area->mPictureHeight = 0x6666;
    area->mStride = stride;
    return area;
}

}  // namespace

extern "C" {

// ---- the writers called by hand. cls and slot pick the object and the virtual (slot 0 555, 1 565, 2 4444, 3 Any, 4 8888, 5 BGRA, the
// order of the vtable). geometry: the point (x, y), the source rectangle (left, top, right, bottom), the bytes in a row and the glyph
// width byte. The destination pixels start at `origin`. Answers the writer's result; the point and the source rectangle are written
// back to geometry (a writer must not change them).
int32_t tpw_fw_writer(int32_t cls, int32_t slot, const uint32_t* areaWords, int32_t stride, uint8_t* origin, int32_t* geometry,
                      const uint8_t* glyph, const uint8_t* colour) {
    LabelObjects();
    TbRenderArea* area = MakeArea(areaWords, stride, origin);
    std::memset(gParamsStorage, 0xa5, sizeof(gParamsStorage));   // (not constructed: only the colour is set)
    TbFontRenderParams* params = reinterpret_cast<TbFontRenderParams*>(gParamsStorage);
    std::memcpy(ColourOf(params), colour, 5);
    tpw_cl_label(kLabelArea, area);
    tpw_cl_label(kLabelParams, params);
    TbPoint point = {geometry[0], geometry[1]};
    TbRect source = {geometry[2], geometry[3], geometry[4], geometry[5]};
    C4BitFontRenderMethodParameters record;
    record.mPoint = &point;
    record.mData = glyph;
    record.mBytesPerRow = geometry[6];
    record.mSource = &source;
    record.mParams = params;
    record.mWidth = static_cast<uint8_t>(geometry[7]);
    Tb4BitFontRenderMethod* method = Object(cls);
    int32_t result = 0;
    switch (slot) {
        case 0: result = static_cast<int32_t>(method->DrawCharTo555(*area, record)); break;
        case 1: result = static_cast<int32_t>(method->DrawCharTo565(*area, record)); break;
        case 2: result = static_cast<int32_t>(method->DrawCharTo4444(*area, record)); break;
        case 3: result = static_cast<int32_t>(method->DrawCharToAny(*area, record)); break;
        case 4: result = static_cast<int32_t>(method->DrawCharTo8888(*area, record)); break;
        default: result = static_cast<int32_t>(method->DrawCharToBGRA(*area, record)); break;
    }
    geometry[0] = point.x;
    geometry[1] = point.y;
    geometry[2] = source.left;
    geometry[3] = source.top;
    geometry[4] = source.right;
    geometry[5] = source.bottom;
    return result;
}

// ---- DrawChar through the table. blend and style pick gFontRenderMethods[blend][4][style]; the glyph is drawn from the font image
// into a render area over `origin`, with the colour. Fills out with: the answer, the drawn rectangle (4), a flag for a missing table
// cell (then nothing else happens). Returns the number of words.
int32_t tpw_fw_drawchar(const uint8_t* image, int32_t imageLength, const uint32_t* areaWords, int32_t stride, uint8_t* origin,
                        const int32_t* clip, int32_t x, int32_t y, int32_t glyph, int32_t blend, int32_t style, const uint8_t* colour,
                        int64_t* out) {
    LabelObjects();
    gImage.assign(image, image + imageLength);
    gImage.resize(imageLength + 128, 0);
    RecFont* font = new (gFontStorage) RecFont;
    font->mData = gImage.data();
    font->mHeader = gImage.data();
    font->mOffsets = reinterpret_cast<uint32_t*>(gImage.data() + 8);
    font->mMBToUnicode = nullptr;
    TbRenderArea* area = MakeArea(areaWords, stride, origin);
    _lbpCurrentRA = area;
    std::memset(gParamsStorage, 0xa5, sizeof(gParamsStorage));
    TbFontRenderParams* params = reinterpret_cast<TbFontRenderParams*>(gParamsStorage);
    params->mState[0] = reinterpret_cast<uintptr_t>(font);
    params->mState[2] = static_cast<uintptr_t>(style);
    params->mState[3] = static_cast<uintptr_t>(blend);
    std::memcpy(ColourOf(params), colour, 5);
    gClip = {clip[0], clip[1], clip[2], clip[3]};
    // The decode buffer starts as a zeroed block of 0x800 bytes (what a glyph's decoder leaves unwritten is then the same on both sides).
    FourBit::FontData& decode = FourBit::FontData::I();
    delete[] decode.mBlock;
    decode.mBlock = new uint8_t[0x800]();
    decode.mSize = 0x800;
    tpw_cl_label(kLabelFont, font);
    tpw_cl_label(kLabelArea, area);
    tpw_cl_label(kLabelParams, params);
    TbFontRenderMethod* method = gFontRenderMethods[blend][4][style];
    if (method == nullptr) {
        out[0] = 0;
        return 1;
    }
    const TbPoint at = {x, y};
    TbRect drawn = {-1, -2, -3, -4};
    const TbError result = method->DrawChar(at, static_cast<uint16_t>(glyph), drawn, *params);
    out[0] = static_cast<int32_t>(result);
    out[1] = drawn.left;
    out[2] = drawn.top;
    out[3] = drawn.right;
    out[4] = drawn.bottom;
    return 5;
}

// ---- the objects: kind 0 builds an object of class cls on the heap and answers its class number (out[0]), kind 1 runs its destructor
// with the hidden flag (a positive halfword deletes the object) and answers the number of operator delete calls (out[1]); this = 0
// for `isNull` (kind 1 only, where the original skips the whole body).
void tpw_fw_life(int32_t cls, int32_t kind, int32_t flag, int32_t isNull, int64_t* out) {
    out[0] = 0;
    out[1] = 0;
    ResetHeap();
    TbFontRenderMethod* method = (kind == 0 || isNull == 0) ? MakeOnHeap(cls) : nullptr;
    ResetHeap();
    if (kind == 0) {
        out[0] = ClassOf(method);
        delete method;
        ResetHeap();
        return;
    }
    const int16_t half = static_cast<int16_t>(flag);
    if (method == nullptr) {
        // The original's skip of the whole body for a NULL this.
    } else if (half > 0) {
        delete method;
    } else {
        // (The destructors end in the base class's, which a virtual destructor reaches by itself; the name is not callable by hand
        // on its own in an optimised build.)
        method->~TbFontRenderMethod();
        ReleaseQuietly(method);
    }
    out[1] = tpw_cl_heap[2];
}

// ---- the table: 30 words, each the class number of the cell's method (0 one colour, 1 one colour without blending, 2 textured, 3
// textured without blending) plus one, 0 for an empty cell.
void tpw_fw_table(int32_t* out) {
    int n = 0;
    for (int blend = 0; blend < 2; ++blend)
        for (int type = 0; type < 5; ++type)
            for (int style = 0; style < 3; ++style) {
                TbFontRenderMethod* const method = gFontRenderMethods[blend][type][style];
                out[n++] = method == nullptr ? 0 : ClassOf(method) + 1;
            }
}

// ---- TbFontRender::Initialise over the real table: parameters (never constructed: storage filled with 0xa5) with a font of the given
// type (the font's GetType answers it), the style and the blend word. out: the answer, and the class number plus one of the method it
// chose (0 for none).
void tpw_fw_initialise(int32_t style, int32_t type, int32_t blend, int64_t* out) {
    LabelObjects();
    TypeFont* font = new (gTypeFontStorage) TypeFont;
    font->type = static_cast<uint32_t>(type);
    std::memset(gParamsStorage, 0xa5, sizeof(gParamsStorage));
    TbFontRenderParams* params = reinterpret_cast<TbFontRenderParams*>(gParamsStorage);
    params->mState[0] = reinterpret_cast<uintptr_t>(font);
    params->mState[2] = static_cast<uintptr_t>(static_cast<uint32_t>(style));
    params->mState[3] = static_cast<uintptr_t>(static_cast<uint32_t>(blend));
    std::memset(gRenderStorage, 0xa5, sizeof(gRenderStorage));
    TbFontRender* render = reinterpret_cast<TbFontRender*>(gRenderStorage);
    tpw_cl_label(kLabelFont, font);
    tpw_cl_label(kLabelParams, params);
    const TbError result = render->Initialise(*params);
    out[0] = static_cast<int32_t>(result);
    out[1] = render->mMethod == nullptr ? 0 : ClassOf(render->mMethod) + 1;
}

// SetWriteZero(value), answering the flag afterwards.
int32_t tpw_fw_writezero(int32_t value) {
    SetWriteZero(value != 0);
    return gWriteZero ? 1 : 0;
}

}  // extern "C"
