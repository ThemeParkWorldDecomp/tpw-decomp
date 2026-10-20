// CMapZoom (include/tb_mapzoom.h): the map zoom's ten methods of the engine library
// (SimTheme Park 2.04b3, the original file is tb_mapzoom.cpp by its assertions; the file of
// the same name here holds the light manager and the map lists the inventory files under
// that name, so the class has a file of its own).
//
//   0x10019884 ~CMapZoom (92)       0x10019904 EnableFiltering (12)   0x10019940 DisableFiltering (12)
//   0x1001997c Init (344)           0x10019afc Close (132)            0x10019ba8 Use (472)
//   0x10019da8 Window (20)          0x10019de8 Update (384)           0x10019f94 Prepare (260)
//   0x1001a0c0 Execute (1,524)
//
// The grid of tiles is an array of texture entries made with new[] (the Mac builds it with
// the compiler's __construct_new_array and frees it with __destroy_new_array, so the
// constructors and destructors of the entries run). Floats follow the listing: single
// precision throughout, the fused multiply-adds through ppc_fmadds, the float to int
// conversions through ppc_fctiwz, and an int to float conversion is the exact (float)int.
#include "tb_mapzoom.h"

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// The bytes of a pixel in each texture format (the file's own copy of the table, 0x10051c30).
const int32_t kBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

constexpr float kSixtyFourth = 0.015625f;     // 0x1004fcdc: 1/64, the tile size
constexpr float kTileSize = 64.0f;            // 0x1004fcd8
constexpr float kInsetPixels = 0.3f;          // 0x1004fcd0: the inset, in texels, against filter bleeding at a page edge
constexpr float kRendererZShift = 0.03f;      // 0x1004fcd4: added to the z with renderer 4
constexpr int32_t kRendererFour = 4;          // gei_System.mRenderer value that shifts the z
constexpr uint32_t kDisplayFlagTwo = 0x2;     // gei_Display.mFlags bit that makes the v inset a whole texel

// Entry flags and sprite bits Execute tests (names invented; the sky's RenderAdd tests the same pair).
constexpr uint32_t kEntryMarksSprite = 0x00100000;   // the sprite gets flag 4
constexpr uint32_t kSpriteFlagFour = 4;
constexpr uint32_t kBlendingOpaque = 0x1256;         // mColour's top byte 0xff
constexpr uint32_t kBlendingTranslucent = 0x556;
constexpr uint32_t kBlendingNoFilter = 0x20000;      // mFiltering 0
constexpr int32_t kClipMode = -3;                    // DrawSprite: every clip plane but the far one
constexpr int32_t kMaxTiles = 32;                    // the original's tables hold 32 pairs

// A signed division by 64 as the listing does it (srawi, addze): truncation toward zero.
int32_t DivideBy64(int32_t value) { return value / 64; }

// The index of a tile's entry; the arithmetic is 32 bits wide as on the Mac.
int32_t TileIndex(int32_t first, int32_t column, int32_t row, int32_t columns) {
    return static_cast<int32_t>(static_cast<uint32_t>(first) + static_cast<uint32_t>(column) +
                                static_cast<uint32_t>(row) * static_cast<uint32_t>(columns));
}

// The tile a map coordinate falls in: the product with 1/64 truncated toward zero.
int32_t TileOf(float coordinate) { return ppc_fctiwz(static_cast<double>(coordinate * kSixtyFourth)); }

// The fraction of the tile the coordinate lands at: (coordinate - tile * 64) / 64.
float FractionIn(float coordinate) {
    const int32_t tile = TileOf(coordinate);
    const int32_t origin = static_cast<int32_t>(static_cast<uint32_t>(tile) << 6);
    return (coordinate - static_cast<float>(origin)) * kSixtyFourth;
}

}  // namespace

// ---- construction, destruction -----------------------------------------------------------

// The part of __sinit_tb_mapzoom_cpp (0x1001a6dc) that builds the global; the rest of the
// object is zero (engine data).
CMapZoom::CMapZoom()
    : mFlags(0),
      mWidth(0),
      mHeight(0),
      mPitch(0),
      mColumns(0),
      mRows(0),
      mF18(0),
      mEntries(nullptr),
      mUseX0(0.0f),
      mUseY0(0.0f),
      mUseX1(0.0f),
      mUseY1(0.0f),
      mWindowX0(0.0f),
      mWindowY0(0.0f),
      mWindowX1(0.0f),
      mWindowY1(0.0f),
      mColour(0xffffffffu),
      mZ(1.0f),
      mFirstEntry(0),
      mF4C(0),
      mF50(0),
      mF54(0),
      mEntryCount(0),
      mFiltering(1) {}

CMapZoom gei_MapZoom;   // 0x100afe20; native static lifetime runs the same constructor and, at exit, the destructor

CMapZoom::~CMapZoom() { Close(); }

void CMapZoom::EnableFiltering() { mFiltering = 1; }

void CMapZoom::DisableFiltering() { mFiltering = 0; }

// ---- the grid ----------------------------------------------------------------------------

int32_t CMapZoom::Init(void* data, int32_t width, int32_t height, int32_t pitch) {
    mWidth = width;
    mHeight = height;
    mPitch = pitch;
    mColumns = DivideBy64(static_cast<int32_t>(static_cast<uint32_t>(width) + 0x3f));
    mRows = DivideBy64(static_cast<int32_t>(static_cast<uint32_t>(height) + 0x3f));
    const int32_t count = static_cast<int32_t>(static_cast<uint32_t>(mColumns) * static_cast<uint32_t>(mRows));
    mEntries = new CTextureEntry[static_cast<size_t>(count)];
    const int32_t bytesPerPixel = kBytesPerPixel[gei_TextureSettings.mFormat];
    const int32_t pixelsPerRow = mPitch / bytesPerPixel;
    mFirstEntry = 0;
    int32_t made = 0;
    int32_t rowStart = 0;   // the pixels of the picture above the row (tiles are 64 rows high)
    for (int32_t row = 0; row < mRows; ++row) {
        int32_t pixel = static_cast<int32_t>(static_cast<uint32_t>(rowStart) << 6);
        for (int32_t column = 0; column < mColumns; ++column) {
            CTextureEntry* entry = mEntries + (row * mColumns + column);
            entry->mFlags = CMapZoom::kEntryFlags;
            entry->SetSize(64, 64);
            const int32_t offset = static_cast<int32_t>(static_cast<uint32_t>(bytesPerPixel) * static_cast<uint32_t>(pixel));
            entry->mPitch = static_cast<int16_t>(pixelsPerRow);
            pixel = static_cast<int32_t>(static_cast<uint32_t>(pixel) + 0x40u);
            entry->mData = static_cast<uint8_t*>(data) + offset;
            ++made;
        }
        rowStart = static_cast<int32_t>(static_cast<uint32_t>(rowStart) + static_cast<uint32_t>(pixelsPerRow));
    }
    mEntryCount = made;
    return 1;
}

int32_t CMapZoom::Close() {
    if (mEntries != nullptr) {
        for (int32_t i = 0; i < mEntryCount; ++i) mEntries[i].mData = nullptr;
        delete[] mEntries;
        mEntries = nullptr;
        mEntryCount = 0;
    }
    return 1;
}

// ---- the rectangles ----------------------------------------------------------------------

void CMapZoom::Use(float x0, float y0, float x1, float y1) {
    const float width = static_cast<float>(mWidth);
    const float height = static_cast<float>(mHeight);
    // QUIRK: the original's text has || where && was meant (between the x tests with y0 >= 0
    // and the rest of the y tests), so most rectangles pass; a failure only reports.
    const bool valid = (x0 >= 0.0f && x0 < width && x1 >= 0.0f && x1 < width && y0 >= 0.0f) ||
                       (y0 < height && y1 >= 0.0f && y1 < height);
    if (!valid) {
        msl_assertion_failed(
            "(xbegin>=0) && (xbegin<md_Width) && (xend>=0) && (xend<md_Width) && (ybegin>=0) || (ybegin<md_Height) && "
            "(yend>=0) && (yend<md_Height)",
            "tb_mapzoom.cpp", 0x7c);
    }
    mFlags |= kUseSet;
    mUseX0 = x0;
    mUseX1 = x1;
    mUseY0 = y0;
    mUseY1 = y1;
}

void CMapZoom::Window(float x0, float y0, float x1, float y1) {
    mWindowX0 = x0;
    mWindowX1 = x1;
    mWindowY0 = y0;
    mWindowY1 = y1;
}

void CMapZoom::Update(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    const bool valid = x0 >= 0 && x0 < mWidth && x1 >= 0 && x1 < mWidth && y0 >= 0 && y0 < mHeight && y1 >= 0 &&
                       y1 < mHeight;
    if (!valid) {
        msl_assertion_failed(
            "(xbegin>=0) && (xbegin<md_Width) && (xend>=0) && (xend<md_Width) && (ybegin>=0) && (ybegin<md_Height) && "
            "(yend>=0) && (yend<md_Height)",
            "tb_mapzoom.cpp", 0x9e);
    }
    const int32_t column0 = DivideBy64(x0);
    const int32_t column1 = DivideBy64(x1);
    const int32_t row1 = DivideBy64(y1);
    for (int32_t row = DivideBy64(y0); row <= row1; ++row) {
        for (int32_t column = column0; column <= column1; ++column) {
            gei_TCache->Remove(mEntries + TileIndex(mFirstEntry, column, row, mColumns));
        }
    }
}

// ---- the frame ---------------------------------------------------------------------------

int32_t CMapZoom::Prepare() {
    if (mEntries == nullptr) {
        msl_assertion_failed("0", "tb_mapzoom.cpp", 0xd8);
        return 0;
    }
    const int32_t column0 = TileOf(mUseX0);
    const int32_t column1 = TileOf(mUseX1);
    const int32_t row1 = TileOf(mUseY1);
    for (int32_t row = TileOf(mUseY0); row <= row1; ++row) {
        for (int32_t column = column0; column <= column1; ++column) {
            gei_TCache->Prepare(mEntries + TileIndex(mFirstEntry, column, row, mColumns), 0.0f);
        }
    }
    return 1;
}

int32_t CMapZoom::Execute() {
    if (mEntries == nullptr) {
        msl_assertion_failed("0", "tb_mapzoom.cpp", 0x147);
        return 0;
    }
    const float screenPerMapX = (mWindowX1 - mWindowX0) / (mUseX1 - mUseX0);
    const float screenPerMapY = (mWindowY1 - mWindowY0) / (mUseY1 - mUseY0);
    const int32_t column0 = TileOf(mUseX0);
    const int32_t column1 = TileOf(mUseX1);
    const int32_t row0 = TileOf(mUseY0);
    const int32_t row1 = TileOf(mUseY1);

    // Per column: the u range the tile shows and the screen x range it covers; per row the
    // same with v and the NEGATED screen y. Pair k belongs to tile column0 + k.
    float screenX[kMaxTiles * 2];
    float screenY[kMaxTiles * 2];
    float u[kMaxTiles * 2];
    float v[kMaxTiles * 2];
    float x = mWindowX0;
    for (int32_t column = column0; column <= column1; ++column) {
        const int32_t k = (column - column0) * 2;
        u[k] = column == column0 ? FractionIn(mUseX0) : 0.0f;
        u[k + 1] = column == column1 ? FractionIn(mUseX1) : 1.0f;
        screenX[k] = x;
        x = ppc_fmadds(kTileSize, screenPerMapX * (u[k + 1] - u[k]), x);
        screenX[k + 1] = x;
    }
    float y = mWindowY0;
    for (int32_t row = row0; row <= row1; ++row) {
        const int32_t k = (row - row0) * 2;
        v[k] = row == row0 ? FractionIn(mUseY0) : 0.0f;
        v[k + 1] = row == row1 ? FractionIn(mUseY1) : 1.0f;
        screenY[k] = ppc_fneg(y);
        y = ppc_fmadds(kTileSize, screenPerMapY * (v[k + 1] - v[k]), y);
        screenY[k + 1] = ppc_fneg(y);
    }

    // The sprite: four corners (x, y, z, w) in the order (left, top), (right, top), (right,
    // bottom), (left, bottom) of the picture. The flags word is a local the original never
    // sets (stack leftovers; the tile loop only ORs bit 4 in): zero here.
    float corners[16];
    SSpriteDrawParameters sprite;
    sprite.mFlags = 0;
    sprite.mVertices = corners;
    sprite.mF08 = -1;
    sprite.mF28 = 2;
    sprite.mF14 = 0;
    const float z = gei_System.mRenderer == kRendererFour ? kRendererZShift + mZ : mZ;
    for (int32_t i = 0; i < 4; ++i) {
        corners[i * 4 + 2] = z;
        corners[i * 4 + 3] = 1.0f;
    }
    for (int32_t row = row0; row <= row1; ++row) {
        const int32_t r = (row - row0) * 2;
        for (int32_t column = column0; column <= column1; ++column) {
            CTextureEntry* entry = mEntries + TileIndex(mFirstEntry, column, row, mColumns);
            sprite.mTexture = (entry->mFlags & CTextureEntry::kOnPage) != 0 ? entry->mTexture : nullptr;
            entry->GetBlendingMode();   // the answer is stored and overwritten below
            if ((entry->mFlags & kEntryMarksSprite) != 0) sprite.mFlags |= kSpriteFlagFour;
            sprite.mBlendingMode = (mColour >> 24) == 0xff ? kBlendingOpaque : kBlendingTranslucent;
            if (mFiltering == 0) sprite.mBlendingMode |= kBlendingNoFilter;
            const int32_t c = (column - column0) * 2;
            corners[0] = screenX[c];
            corners[12] = corners[0];
            corners[4] = screenX[c + 1];
            corners[8] = corners[4];
            corners[1] = screenY[r + 1];
            corners[5] = corners[1];
            corners[9] = screenY[r];
            corners[13] = corners[9];
            sprite.mU0 = ppc_fmadds(u[c], entry->mUSize, entry->mUOffset);
            sprite.mU1 = ppc_fmadds(u[c + 1], entry->mUSize, entry->mUOffset);
            sprite.mV1 = ppc_fmadds(v[r], entry->mVSize, entry->mVOffset);
            sprite.mV0 = ppc_fmadds(v[r + 1], entry->mVSize, entry->mVOffset);
            // A tile on a page is pulled in by 0.3 of a texel (a whole texel in v with
            // display flag 2) so that the filter does not read the neighbouring tile.
            CTextureBase* page = entry->mPage != nullptr ? entry->mPage->mTexture : nullptr;
            if (page != nullptr) {
                const float du = kInsetPixels / static_cast<float>(page->mWidth);
                sprite.mU0 = sprite.mU0 + du;
                sprite.mU1 = sprite.mU1 - du;
                const float numerator = (gei_Display.mFlags & kDisplayFlagTwo) != 0 ? 1.0f : kInsetPixels;
                const float dv = numerator / static_cast<float>(page->mHeight);
                sprite.mV0 = sprite.mV0 - dv;
                sprite.mV1 = sprite.mV1 + dv;
            }
            if (gei_DrawPrimitive->DrawSprite(&sprite, kClipMode) == 0) return 0;
        }
    }
    return 1;
}
