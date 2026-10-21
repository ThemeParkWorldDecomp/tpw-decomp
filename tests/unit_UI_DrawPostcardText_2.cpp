// Test library for the ui_utils helpers (unit v51012; libtpw_unit_UI_DrawPostcardText_2.dylib, tests/test_UI_DrawPostcardText_2.py), main app:
//   src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_utils.cpp   UI_DrawPostcardText, __sinit_ui_utils_cpp, MakeRenderAreaFromRAW,
//   InterfaceDrawEngineHGradient::CalcUVs, InterfaceVisitorTimeTextObject::Change
//
// The callees outside the unit are recording fakes (UI_DrawText, LbPalette_FindColour, the font parameters' two calls, the render
// area's constructor, TbITable::FormatString, InterfaceTextObjectSprite::Change). The globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "LbPalette.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_Postcard.h"

using namespace calllog;

// ---- globals of other files, defined here
struct TbPalette {
    uint8_t mBytes[0x400];
};
TbPalette _lbGlobalPalette;
TbRenderArea gUIRenderArea{TbRenderArea::kCleared};
int32_t gUIRenderAreaInUse;
CText gText;

// ---- the fakes
uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    ev("FindColour", {lab(palette), red, green, blue});
    return static_cast<uint8_t>(ret("FindColour"));
}

// CText's constructor and destructor reach these (their bodies are not under test; nothing here is logged).
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}

TbError TbFontRenderParams::UseBlendedMode() {
    ev("UseBlendedMode", {lab(this)});
    return TbError(0);
}

TbError TbFontRenderParams::UseAlphaWriteMode() {
    ev("UseAlphaWriteMode", {lab(this)});
    return TbError(0);
}

TbRenderArea::TbRenderArea() { ev("TbRenderArea", {lab(this)}); }

void UI_DrawText(const char16_t* text, const TbRect& rect, const TbColour& colour, int8_t font, int8_t horizontal, int8_t vertical,
                 int32_t wrap, TbRect* extent, int32_t onCurrent) {
    const int64_t packed = (static_cast<int64_t>(colour.mAlpha) << 24) | (colour.mRed << 16) | (colour.mGreen << 8) | colour.mBlue;
    ev("UI_DrawText", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(text) & 0xffffffffu), rect.left, rect.top, rect.right, rect.bottom,
                       packed, colour.mIndex, static_cast<uint8_t>(font), static_cast<uint8_t>(horizontal), static_cast<uint8_t>(vertical),
                       wrap, lab(extent)});
    uint32_t masks[4];
    std::memcpy(masks, gUIRenderArea.mF00, sizeof(masks));
    ev("UI_DrawText2", {onCurrent, gUIRenderAreaInUse, masks[0], masks[1], masks[2], masks[3], gUIRenderArea.mDepth,
                        gUIRenderArea.mPalettised,
                        static_cast<int64_t>(reinterpret_cast<uintptr_t>(gUIRenderArea.mData) & 0xffffffffu), gUIRenderArea.mWidth,
                        gUIRenderArea.mPictureHeight, gUIRenderArea.mStride});
}

int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// Writes "F", a letter of the string id, two letters of the value, the flag, a letter of the tag id.
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbVTag* tag = va_arg(args, TbVTag*);
    va_end(args);
    RideAgeTag* age = static_cast<RideAgeTag*>(tag);
    ev("FormatString", {lab(this), size, id, age->mMagic, age->mId, age->mValue, age->mNegative, 1});
    out[0] = 0x46;
    out[1] = static_cast<char16_t>(0x41 + (id & 0xf));
    out[2] = static_cast<char16_t>(0x61 + (age->mValue & 15));
    out[3] = static_cast<char16_t>(0x61 + ((age->mValue >> 4) & 15));
    out[4] = static_cast<char16_t>(0x30 + (age->mNegative & 1));
    out[5] = static_cast<char16_t>(0x61 + (age->mId & 15));
    out[6] = 0;
    return 0;
}

// The vtable of InterfaceVisitorTimeTextObject (emitted by the unit's source, which defines its first virtual) names every virtual of its
// bases: empty bodies here, none is called by the test.
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 0; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
TbTextFormatter::~TbTextFormatter() {}
TbFontRender::~TbFontRender() {}
SSpriteEngineData::~SSpriteEngineData() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}

void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    if (reinterpret_cast<uintptr_t>(change) == 0x30000000u) {
        ev("SpriteChange", {lab(this), flags, lab(change), 0, 0, 0, 0, 0, 0});
        return;
    }
    const char16_t* text = change->mText->mText;
    int64_t c[5] = {0, 0, 0, 0, 0};
    int64_t n = 0;
    while (text[n] != 0 && n < 0x40) {
        if (n < 5) c[n] = text[n];
        ++n;
    }
    ev("SpriteChange", {lab(this), flags, 0, c[0], c[1], c[2], c[3], c[4], n});
}

extern "C" {

uint64_t tpw_ui2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"area", &gUIRenderArea},
        {"inuse", &gUIRenderAreaInUse},
        {"colour", gUIUtilsColour},
        {"palette", &_lbGlobalPalette},
        {"rparams", &gText.mRenderParams},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ui2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ui2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sUVSet));
    return n;
}

// The render area as the Mac's ten words: masks, depth, palettised, pixels (the low 32 bits), width, height, stride.
void tpw_ui2_area_set(const uint32_t* w) {
    std::memcpy(gUIRenderArea.mF00, w, 16);
    gUIRenderArea.mDepth = static_cast<int32_t>(w[4]);
    gUIRenderArea.mPalettised = static_cast<int32_t>(w[5]);
    gUIRenderArea.mData = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(w[6]));
    gUIRenderArea.mWidth = static_cast<int32_t>(w[7]);
    gUIRenderArea.mPictureHeight = static_cast<int32_t>(w[8]);
    gUIRenderArea.mStride = static_cast<int32_t>(w[9]);
}

void tpw_ui2_area_get(uint32_t* w) {
    std::memcpy(w, gUIRenderArea.mF00, 16);
    w[4] = static_cast<uint32_t>(gUIRenderArea.mDepth);
    w[5] = static_cast<uint32_t>(gUIRenderArea.mPalettised);
    w[6] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gUIRenderArea.mData));
    w[7] = static_cast<uint32_t>(gUIRenderArea.mWidth);
    w[8] = static_cast<uint32_t>(gUIRenderArea.mPictureHeight);
    w[9] = static_cast<uint32_t>(gUIRenderArea.mStride);
}

void tpw_ui2_table_set(uint64_t table) { gText.mResourcePointers[6] = reinterpret_cast<void*>(static_cast<uintptr_t>(table)); }

void tpw_ui2_postcard(uint64_t text, uint64_t pixels, int32_t width, int32_t height, int32_t left, int32_t top, int32_t right,
                      int32_t bottom, uint32_t colour, int32_t extra) {
    SamsUtilities::URectangle::LongRect rect{left, top, right, bottom};
    UI_DrawPostcardText(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)), reinterpret_cast<void*>(static_cast<uintptr_t>(pixels)),
                        width, height, rect, colour, extra);
}

void tpw_ui2_sinit() { __sinit_ui_utils_cpp(); }

void tpw_ui2_raw(uint64_t pixels, int32_t width, int32_t height, int32_t type, int32_t useArea) {
    MakeRenderAreaFromRAW(reinterpret_cast<void*>(static_cast<uintptr_t>(pixels)), width, height, static_cast<RAWSurfaceType>(type),
                          useArea ? &gUIRenderArea : nullptr);
}

void tpw_ui2_calcuvs(uint32_t fraction, uint64_t uvs) {
    uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(sizeof(InterfaceDrawEngineHGradient)));
    InterfaceDrawEngineHGradient* self = reinterpret_cast<InterfaceDrawEngineHGradient*>(block);
    std::memcpy(&self->mFraction, &fraction, 4);
    self->mMeshSet.mUVSets = reinterpret_cast<sUVSet*>(static_cast<uintptr_t>(uvs));
    self->CalcUVs();
}

void tpw_ui2_time(uint64_t self, uint32_t flags, uint64_t change) {
    reinterpret_cast<InterfaceVisitorTimeTextObject*>(static_cast<uintptr_t>(self))
        ->InterfaceVisitorTimeTextObject::Change(flags, reinterpret_cast<DOChange*>(static_cast<uintptr_t>(change)));
}

}  // extern "C"
