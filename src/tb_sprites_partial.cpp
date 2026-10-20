// tb_sprites_partial.cpp: the partial sprite draw (engine library of SimTheme Park 2.04b3; the original file is
// tb_sprite.cpp by its assertions, the inventory files the range under tb_starfield.cpp):
//
//   0x10030b48 SSpriteGameData::DrawPartial   (1,296 bytes)
//
// A partial sprite is a pixel block (SSpriteEngineData::Init(int, int): the interface's text objects draw their text
// into one) cut into texture entries of the cache's UI texture size by GeneratePartialTexture. DrawPartial puts the
// block on the screen: one quad for each entry, row by row, through CDrawPrimitive::DrawSprite. CSpriteManager::
// RenderPartialSprites calls it for every partial sprite the frame added.
#include <cstdint>

#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_resource_display.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// SSpriteGameData::mF10 (names invented): where the picture hangs from the sprite's position, vertically (any of the
// three bits asks for an offset; the middle bit wins over the lower one) and horizontally.
constexpr uint32_t kHangVertical = 0x07000000;
constexpr uint32_t kHangMiddleY = 0x02000000;   // half the picture's height up
constexpr uint32_t kHangBottom = 0x04000000;    // all of it
constexpr uint32_t kHangHorizontal = 0x70000000;
constexpr uint32_t kHangMiddleX = 0x20000000;   // half the picture's width to the left
constexpr uint32_t kHangRight = 0x40000000;     // all of it

// The texture entry's flag that says it is on a cache page, and the one the sprite code copies to flag 4 of the
// draw record (the sky and the particles do the same; DrawSprite does not read that bit).
constexpr uint32_t kTextureOnPage = 0x00001000;
constexpr uint32_t kTextureFlag100000 = 0x00100000;

// SSpriteDrawParameters::mFlags: 0x400 marks the primitive for round-off (DrawSprite).
constexpr uint32_t kSpriteRoundOff = 0x400;
constexpr uint32_t kSpriteFlag4 = 4;
// The projection type of the sprite's depth (CTransformBase::Project's last argument).
constexpr int32_t kProjectionType = 2;
// The blending mode every partial quad is drawn with, whatever its texture entry says (the entry's own
// GetBlendingMode is called and dropped).
constexpr uint32_t kPartialBlendingMode = 0x07060556;

// The integer to single conversion of the original's (exact for the halfword sizes, rounded to nearest otherwise).
float ToFloat(int32_t value) { return static_cast<float>(value); }

// Twice the sum of two pixel counts, in 32 bits (a pixel is two steps of the screen's 2 / size).
float TwiceToFloat(int32_t a, int32_t b) {
    return ToFloat(static_cast<int32_t>((static_cast<uint32_t>(a) + static_cast<uint32_t>(b)) << 1));
}

}  // namespace

// Quirks kept. The answer is always 1: a DrawSprite that fails only ends the drawing. The instance's position is
// taken in the screen's -1..1 space with y turned over (down is positive), the picture hangs from it by the instance's
// two halfwords at 0x30 and 0x32 (mUsedWidth, mUsedHeight: the picture's size on the screen, which the hang flags
// use and the height also always lifts the picture by, so the position is its bottom left corner). The quads walk the
// entry array in order, so a texture of fewer entries than the used part has cells would be read past its end, and a NULL texture
// (all four texture coordinates zero) makes the following cells read from the address one entry past NULL (the
// original does the same arithmetic): such a sprite has at most one cell. The cache's UI texture size is
// read once (the original reads it three times; nothing in between can change it) and divides without a
// guard, as the original's divw does. The record handed to DrawSprite is one and the same for every quad: flag 4
// stays set once an entry asked for it, and the colour DrawSprite masks is masked again for the next quad.
// The blending mode is set to the constant after every texture entry's own is worked out (and dropped).
int32_t SSpriteGameData::DrawPartial() {
    SSpriteEngineData* sprite = mSprite;
    const int32_t cell = gei_TCache->mUITextureSize;
    const int32_t usedWidth = sprite->mUsedWidth;
    const int32_t usedHeight = sprite->mUsedHeight;
    CTextureEntry* entry = sprite->mTexture;

    float corners[16];
    SSpriteDrawParameters draw{};
    draw.mFlags = kSpriteRoundOff;
    draw.mVertices = corners;
    draw.mF08 = mF24;
    draw.mF28 = kProjectionType;

    // The cells: the used part divided by the cell size, rounded up.
    const uint32_t cellLess = static_cast<uint32_t>(cell) - 1u;
    const int32_t columns = static_cast<int32_t>(static_cast<uint32_t>(usedWidth) + cellLess) / cell;
    const int32_t rows = static_cast<int32_t>(static_cast<uint32_t>(usedHeight) + cellLess) / cell;

    // The picture's top left corner in the screen's space: the position with y turned over, moved by the hang flags
    // and lifted by the instance's height (in pixels, times the scale, as a fraction of the display's height).
    const uint32_t flags = static_cast<uint32_t>(mF10);
    const float scale = mF2C;
    float x = mPosition.x;
    float y = -mPosition.y;
    const float z = mPosition.z;
    if ((flags & kHangVertical) != 0) {
        const float hang = 2.0f * (ToFloat(mUsedHeight) * (scale / ToFloat(sprite->mHeight)));
        if ((flags & kHangMiddleY) != 0) {
            y = ppc_fmadds(0.5f, hang, y);
        } else if ((flags & kHangBottom) != 0) {
            y = y + hang;
        }
    }
    y = ppc_fnmsubs((2.0f * scale) / ToFloat(gei_Display.mHeight), ToFloat(mUsedHeight), y);
    if ((flags & kHangHorizontal) != 0) {
        const float hang = 2.0f * (gei_Display.mF04 * (ToFloat(mUsedWidth) * (scale / ToFloat(sprite->mWidth))));
        if ((flags & kHangMiddleX) != 0) {
            x = ppc_fnmsubs(0.5f, hang, x);
        } else if ((flags & kHangRight) != 0) {
            x = x - hang;
        }
    }

    // The screen space is -1..1; a pixel is two steps of 1 / width or 1 / height.
    const float inverseWidth = 1.0f / ToFloat(gei_Display.mWidth);
    const float inverseHeight = 1.0f / ToFloat(gei_Display.mHeight);
    const float top = 1.0f + y;
    const float left = 1.0f + x;
    int32_t pixelY = ppc_fctiwz(0.5f * (top * ToFloat(gei_Display.mHeight)));

    for (int32_t row = 0; row < rows; ++row) {
        const float y0 = ppc_fmsubs(inverseHeight, TwiceToFloat(pixelY, 0), 1.0f);
        int32_t pixelX = ppc_fctiwz(0.5f * (left * ToFloat(gei_Display.mWidth)));
        for (int32_t column = 0; column < columns; ++column) {
            if (entry == nullptr) {
                draw.mTexture = nullptr;
                draw.mU0 = 0.0f;
                draw.mV0 = 0.0f;
                draw.mU1 = 0.0f;
                draw.mV1 = 0.0f;
            } else {
                draw.mTexture = (entry->mFlags & kTextureOnPage) != 0 ? entry->mTexture : nullptr;
                entry->GetBlendingMode();
                draw.mU0 = entry->mUOffset;
                draw.mV0 = entry->mVOffset;
                draw.mU1 = draw.mU0 + entry->mUSize;
                draw.mV1 = draw.mV0 + entry->mVSize;
                if ((entry->mFlags & kTextureFlag100000) != 0) draw.mFlags |= kSpriteFlag4;
            }
            draw.mBlendingMode = kPartialBlendingMode;

            // The cell's size in pixels: the rest of the used part at the last column and row, else the cell size.
            int32_t width = static_cast<int32_t>(static_cast<uint32_t>(usedWidth) -
                                                 static_cast<uint32_t>(column) * static_cast<uint32_t>(cell));
            int32_t height = static_cast<int32_t>(static_cast<uint32_t>(usedHeight) -
                                                  static_cast<uint32_t>(row) * static_cast<uint32_t>(cell));
            if (width > cell) width = cell;
            if (height > cell) height = cell;

            const float x0 = ppc_fmsubs(inverseWidth, TwiceToFloat(pixelX, 0), 1.0f);
            const float x1 = ppc_fmsubs(inverseWidth, TwiceToFloat(pixelX, width), 1.0f);
            const float y1 = ppc_fmsubs(inverseHeight, TwiceToFloat(pixelY, height), 1.0f);
            // The corners, clockwise from the top left: x, y, z, w.
            corners[0] = x0;
            corners[1] = y0;
            corners[2] = z;
            corners[3] = 1.0f;
            corners[4] = x1;
            corners[5] = y0;
            corners[6] = z;
            corners[7] = 1.0f;
            corners[8] = x1;
            corners[9] = y1;
            corners[10] = z;
            corners[11] = 1.0f;
            corners[12] = x0;
            corners[13] = y1;
            corners[14] = z;
            corners[15] = 1.0f;
            if (gei_DrawPrimitive->DrawSprite(&draw, -1) == 0) return 1;

            pixelX += cell;
            ++entry;
        }
        pixelY += cell;
    }
    return 1;
}
