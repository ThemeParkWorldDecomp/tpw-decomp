// Test library for the twelve textured glyph writers (libtpw_unit_FontRenderTextured.dylib; see tests/test_FontRenderTextured.py,
// unit w45c): src/ltms_fontrender_textured.cpp (TbTextured4BitFontRenderMethod, blending) and src/ltms_fontrender_textured_noblend.cpp
// (TbTexturedNoBlend4BitFontRenderMethod), linked with the method objects and the one-colour writers (unit w45b) and the render method's
// core (unit w45a), the real font (src/TbIRLE4bitFont.cpp), the render parameters (src/ltms_fontrender.cpp, src/tb_fontrenderparams.cpp)
// and TbPixFormat32 (src/TbPixFmt.cpp), so that DrawChar can be driven through the table end to end with style 2.
//
// Fakes (each records the same event as the PowerPC side's stub, through calllog): LbDraw_GetClipRect (answers a case-owned clip
// rectangle), TbRect::Intersection (the formula of the listing, recording its operands), the font's AddRef and Release.
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

constexpr int kLabelObject = 50;

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

void LabelObjects() {
    for (int cls = 0; cls < 4; ++cls) tpw_cl_label(kLabelObject + cls, Object(cls));
}

// The case's render area, parameters and font live here.
constexpr int kLabelFont = 10, kLabelArea = 20, kLabelParams = 30;
std::vector<uint8_t> gImage;
alignas(RecFont) unsigned char gFontStorage[sizeof(RecFont)];
alignas(TbRenderArea) unsigned char gAreaStorage[sizeof(TbRenderArea)];
alignas(TbFontRenderParams) unsigned char gParamsStorage[sizeof(TbFontRenderParams)];

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


// The case's texture: the render parameters hold it at +0x14..+0x3b (a TbRenderArea made of the six format words, the pixels, the
// width, the height and the bytes in a row) and its origin at +0x3c and +0x40. `spec` = the six format words, the width, the height,
// the bytes in a row, the origin x and y.
void SetTexture(TbFontRenderParams* params, const int32_t* spec, uint8_t* texels) {
    TbRenderArea* texture = reinterpret_cast<TbRenderArea*>(&params->mState[4]);
    std::memset(texture, 0, sizeof(*texture));
    std::memcpy(texture->mF00, spec, 16);
    texture->mDepth = spec[4];
    texture->mPalettised = spec[5];
    texture->mData = texels;
    texture->mWidth = spec[6];
    texture->mPictureHeight = spec[7];
    texture->mStride = spec[8];
    params->mState[14] = static_cast<uint32_t>(spec[9]);
    params->mState[15] = static_cast<uint32_t>(spec[10]);
}

// TbPixFormat32 as the case wants it (bullfrog 0x10073e88: the four masks, the depth, the palettised flag); the writers compare the
// texture's six format words with it.
void PlantPixFormat(const uint32_t* words) {
    TbPixFormat32.mRedMask = words[0];
    TbPixFormat32.mGreenMask = words[1];
    TbPixFormat32.mBlueMask = words[2];
    TbPixFormat32.mAlphaMask = words[3];
    TbPixFormat32.mBitsPerPixel = static_cast<int32_t>(words[4]);
    TbPixFormat32.mPalettised = static_cast<int32_t>(words[5]);
}

}  // namespace

extern "C" {

// ---- the writers called by hand. cls (2 textured, 3 textured without blending) and slot pick the object and the virtual (slot 0 555,
// 1 565, 2 4444, 3 Any, 4 8888, 5 BGRA, the order of the vtable). geometry: the point (x, y), the source rectangle (left, top, right,
// bottom), the bytes in a row and the glyph width byte. The destination pixels start at `origin`. Answers the writer's result; the point
// and the source rectangle are written back to geometry (a writer must not change them).
int32_t tpw_ft_writer(int32_t cls, int32_t slot, const uint32_t* areaWords, int32_t stride, uint8_t* origin, int32_t* geometry,
                      const uint8_t* glyph, const int32_t* textureSpec, uint8_t* texels, const uint32_t* pixelFormat, void*) {
    LabelObjects();
    PlantPixFormat(pixelFormat);
    TbRenderArea* area = MakeArea(areaWords, stride, origin);
    std::memset(gParamsStorage, 0xa5, sizeof(gParamsStorage));   // (not constructed: only the texture is set)
    TbFontRenderParams* params = reinterpret_cast<TbFontRenderParams*>(gParamsStorage);
    SetTexture(params, textureSpec, texels);
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

// ---- DrawChar through the table with style 2 (the texture is in the parameters). blend and style pick gFontRenderMethods[blend][4][style];
// the glyph is drawn from the font image into a render area over `origin`. Fills out with: the answer and the drawn rectangle (4).
// Returns the number of words (1 for a missing table cell).
int32_t tpw_ft_drawchar(const uint8_t* image, int32_t imageLength, const uint32_t* areaWords, int32_t stride, uint8_t* origin,
                        const int32_t* clip, int32_t x, int32_t y, int32_t glyph, int32_t blend, int32_t style, const int32_t* textureSpec,
                        uint8_t* texels, const uint32_t* pixelFormat, int64_t* out) {
    LabelObjects();
    PlantPixFormat(pixelFormat);
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
    SetTexture(params, textureSpec, texels);
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

}  // extern "C"
