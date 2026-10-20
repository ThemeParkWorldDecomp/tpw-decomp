// tb_sprites_shadow.cpp: the sprites' shadows (engine library of SimTheme Park 2.04b3; the original file is
// tb_sprite.cpp by its assertions, the inventory files the range under tb_starfield.cpp):
//
//   0x1002fa14 SSpriteGameData::CreateShadow      (1,220 bytes)
//   0x1002ff20 SSpriteGameData::DrawRegularShadow   (692)
//   0x10030210 SSpriteGameData::AddRegularShadow    (468)
//
// A regular sprite that has flags 0x200 and 0x10000000 casts a shadow for every light source of the shadow object
// (gShadow, src/tb_shadow.cpp): the sprite's picture, flattened onto the ground by the light's matrix, lying on the height
// field (the ground's height under each of its four corners). CreateShadow works the four corners out;
// DrawRegularShadow draws each shadow as a primitive of its own (the sprite manager's draw-each-sprite path) and
// AddRegularShadow adds them to the primitive being built (the grouped path; the manager's blob shadow texture may take
// the sprite's place).
#include <cstdint>

#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "sprite_engine.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_shadow.h"
#include "tb_starfield.h"
#include "tb_textureentry.h"

namespace {

// SSpriteGameData::mF10 (names invented).
constexpr uint32_t kFlagFlipX = 0x00000010;       // the shadow's two horizontal extents change sign
constexpr uint32_t kFlagFlipZ = 0x00000020;       // and the two along the ground
constexpr uint32_t kFlagTurned = 0x00000080;      // the corners are turned about the vertical axis by mRotation
constexpr uint32_t kFlagCasts = 0x00000200;       // the sprite casts a shadow (both these two flags are needed) ...
constexpr uint32_t kFlagLifted = 0x00000800;      // mShadowLift is added to the ground height
constexpr uint32_t kFlagShadows = 0x10000000;     // ... and has shadows at all
constexpr uint32_t kFlagBlend20000 = 0x00020000;  // the shadow's blending mode gets this bit too

// gei_Display.mFlags: each shadow takes its picture from the sprite file's frames (mFrameFile), not from the sprite.
constexpr uint32_t kDisplayShadowFrames = 0x00100000;

// The texture entry's flag that says it is on a cache page, and the one the sprite code copies to flag 4 of the
// draw record (the sky and the particles do the same).
constexpr uint32_t kTextureOnPage = 0x00001000;
constexpr uint32_t kTextureFlag100000 = 0x00100000;
constexpr uint32_t kSpriteFlag4 = 4;

// The blending mode of a sprite without a texture entry, and the bits every shadow adds to its entry's (0x200, the
// semitransparent bit, is taken off again before each shadow is drawn).
constexpr uint32_t kNoTextureBlendingMode = 0x1256;
constexpr uint32_t kShadowBlendingBit = 0x00000800;
constexpr uint32_t kBlendingSemitransparent = 0x00000200;

// The corners of a shadow are scaled by 20 times the sprite's scale, as the original's constant (engine data 0x1004ff90).
constexpr float kShadowSize = 20.0f;

// The integer to single conversion of the original's (exact for halfwords).
float ToFloat(int32_t value) { return static_cast<float>(value); }

// The draw record's page, blending mode, picture corners and flag 4 for a texture entry; no entry is a picture of nothing.
void TakeTexture(SSpriteDrawParameters& draw, CTextureEntry* entry) {
    if (entry == nullptr) {
        draw.mTexture = nullptr;
        draw.mBlendingMode = kNoTextureBlendingMode;
        draw.mU0 = 0.0f;
        draw.mV0 = 0.0f;
        draw.mU1 = 0.0f;
        draw.mV1 = 0.0f;
        return;
    }
    draw.mTexture = (entry->mFlags & kTextureOnPage) != 0 ? entry->mTexture : nullptr;
    draw.mBlendingMode = entry->GetBlendingMode();
    draw.mU0 = entry->mUOffset;
    draw.mV0 = entry->mVOffset;
    draw.mU1 = draw.mU0 + entry->mUSize;
    draw.mV1 = draw.mV0 + entry->mVSize;
    if ((entry->mFlags & kTextureFlag100000) != 0) draw.mFlags |= kSpriteFlag4;
}

// The blending bits a shadow adds to its texture's.
void AddShadowBlending(SSpriteDrawParameters& draw, uint32_t spriteFlags) {
    draw.mBlendingMode |= kShadowBlendingBit;
    if ((spriteFlags & kFlagBlend20000) != 0) draw.mBlendingMode |= kFlagBlend20000;
}

}  // namespace

// The shadow's four corners, in the order of the picture's corners (the first vertex is the far left, the
// second the far right, the third the near right, the fourth the near left; here "far" is the side of the used part's top):
// the sprite's rectangle (the offsets and size of its sprite, over its used size) scaled by 20 times the
// sprite's scale, the display's aspect (mF04) and the caller's two scales, stretched along the light's cross
// product (gShadow.mLightCross: the ground's direction at right angles to the light) and the ground's up
// axis (y of gUpVector) and placed at the sprite's position. The vertices are then lowered so that the lowest
// is at the position's height, turned by mRotation when flag 0x80 asks, moved by the shadow matrix, laid on the ground
// (each y gets the height field's height under its x and z, a triangle interpolation of the cell, the lift of flag 0x800
// and the display's constant mF938) and moved again by the final matrix. Answers the light's shade.
//
// Quirks kept. The ground lookup does not clamp the cell: it clamps only the neighbour (to the grid size, one past
// the last sample), so a corner off the field reads samples around the table, as the original does. The sprite's
// offset y is the halfword the partial sprites use as a part count (it is a file sprite's here). The math unit is
// asserted (tb_matrix.h line 152) before each of its two uses, which the original continues past. Not reproduced:
// the stack word the original leaves in the draw record's last field in DrawRegularShadow is zero here.
int32_t SSpriteGameData::CreateShadow(SVertex* vertices, CMatrix* shadowMatrix, float scaleX, float scaleZ) {
    SSpriteEngineData* sprite = mSprite;
    const int32_t offsetX = sprite->mOffsetX;
    const int32_t offsetY = sprite->mOffsetY;
    const int32_t width = sprite->mWidth;
    const int32_t height = sprite->mHeight;
    const float usedWidth = ToFloat(sprite->mUsedWidth);
    const float usedHeight = ToFloat(sprite->mUsedHeight);
    const int32_t light = gShadow.mCurrent;

    const float size = kShadowSize * mF2C;
    const float widthSize = size * gei_Display.mF04;
    const float up = gUpVector.y;
    float farZ = scaleZ * (up * (size * (ToFloat(offsetY + height) / usedHeight)));
    float nearZ = scaleZ * (up * (size * (ToFloat(offsetY) / usedHeight)));
    float rightX = scaleX * (widthSize * (ToFloat(offsetX + width) / usedWidth));
    float leftX = scaleX * (widthSize * (ToFloat(offsetX) / usedWidth));
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagFlipX) != 0) {
        leftX = -leftX;
        rightX = -rightX;
    }

    const D3DVECTOR& cross = gShadow.mLightCross[light];
    const float leftAlongX = ppc_fmadds(leftX, cross.x, mPosition.x);
    vertices[0].x = leftAlongX;
    vertices[3].x = leftAlongX;
    const float rightAlongX = ppc_fmadds(rightX, cross.x, mPosition.x);
    vertices[1].x = rightAlongX;
    vertices[2].x = rightAlongX;
    const float leftAlongZ = ppc_fmadds(leftX, cross.z, mPosition.z);
    vertices[0].z = leftAlongZ;
    vertices[3].z = leftAlongZ;
    const float rightAlongZ = ppc_fmadds(rightX, cross.z, mPosition.z);
    vertices[1].z = rightAlongZ;
    vertices[2].z = rightAlongZ;

    if ((flags & kFlagFlipZ) != 0) {
        nearZ = -nearZ;
        farZ = -farZ;
    }
    const float farY = mPosition.y + farZ;
    vertices[1].y = farY;
    vertices[0].y = farY;
    const float nearY = mPosition.y + nearZ;
    vertices[3].y = nearY;
    vertices[2].y = nearY;

    // The lowest corner is at the position's height.
    float lowest = vertices[0].y;
    if (vertices[1].y < lowest) lowest = vertices[1].y;
    if (vertices[2].y < lowest) lowest = vertices[2].y;
    if (vertices[3].y < lowest) lowest = vertices[3].y;
    vertices[0].y = vertices[0].y - lowest;
    vertices[1].y = vertices[1].y - lowest;
    vertices[2].y = vertices[2].y - lowest;
    vertices[3].y = vertices[3].y - lowest;

    if ((flags & kFlagTurned) != 0) RotateVertex_Y(vertices, -mRotation, nullptr);

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(shadowMatrix, vertices, vertices, 0x10, 0x10, 4, 0);

    // The ground under each corner, last corner first.
    const float lift = (flags & kFlagLifted) != 0 ? mShadowLift : 0.0f;
    const float base = lift + gei_Display.mF938;
    for (int32_t i = 3; i >= 0; --i) {
        SVertex& corner = vertices[i];
        CHField* field = gei_HField;
        sHField* grid = field->mF00;
        const float x = corner.x;
        const float z = corner.z;
        const float originX = grid->mF08[0];
        const float originZ = grid->mF08[1];
        const int32_t columns = field->mGridWidth;
        const int32_t cellX = ppc_fctiwz(field->mInvCellX * (x - originX));
        const int32_t cellZ = ppc_fctiwz(field->mInvCellZ * (z - originZ));
        // The next cell along each axis, kept in 0..the grid's size.
        int32_t nextX = cellX + 1;
        if (nextX > columns) {
            nextX = columns;
        } else if (nextX < 0) {
            nextX = 0;
        }
        const int32_t rows = field->mGridHeight;
        int32_t nextZ = cellZ + 1;
        if (nextZ > rows) {
            nextZ = rows;
        } else if (nextZ < 0) {
            nextZ = 0;
        }
        const float* heights = grid->mHeights;
        const int32_t rowStart = columns * cellZ;
        const float corner00 = heights[cellX + rowStart];
        const float corner10 = heights[nextX + rowStart];
        const float corner01 = heights[cellX + columns * nextZ];
        // How far into the cell the corner is, as a fraction of it.
        const float fractionX = field->mInvCellX * (x - ppc_fmadds(ToFloat(cellX), grid->mF08[2], originX));
        const float fractionZ = field->mInvCellZ * (z - ppc_fmadds(ToFloat(cellZ), grid->mF08[3], originZ));
        float ground = ppc_fmadds(fractionX, corner10 - corner00, corner00);
        ground = ppc_fmadds(fractionZ, corner01 - corner00, ground);
        corner.y = corner.y + (base + ground);
    }

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(gei_FinalMatrix, vertices, vertices, 0x10, 0x10, 4, 0);
    return static_cast<int32_t>(gShadow.mLightShades[gShadow.mCurrent]);
}

// Draws the shadows one by one, DrawSprite each (a primitive of its own). Answers 1 for a sprite that casts none, and 0 as
// soon as a DrawSprite fails. Quirks kept. The sprite's own picture is worked out once, before the shadows, with the fog
// flag 4 of its entry in the record for all of them; with the display's flag 0x100000 each shadow takes the entry of its own frame
// instead (the frame of the sprite's file: mFrameBase plus the shadow's byte of mLightFrames, the first sprite of the file when that
// is not below the file's count) and the record's blending mode then loses the bits 0x800 and 0x20000 the code adds
// before the loop (the loop sets the mode afresh; 0x200 is taken off each time as before). The record's last field
// is stack garbage in the original (zero here).
int32_t SSpriteGameData::DrawRegularShadow() {
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagCasts) == 0) return 1;
    if ((flags & kFlagShadows) == 0) return 1;

    SVertex corners[4];
    SSpriteDrawParameters draw{};
    draw.mVertices = corners;
    if ((gei_Display.mFlags & kDisplayShadowFrames) == 0) TakeTexture(draw, mSprite->mTexture);
    AddShadowBlending(draw, static_cast<uint32_t>(mF10));

    CMatrix* matrix = gShadow.GetFirstShadowMatrix();
    int32_t index = 0;
    while (matrix != nullptr) {
        if ((gei_Display.mFlags & kDisplayShadowFrames) != 0) {
            CSpriteFileIO* file = mFrameFile;
            const int32_t frame = static_cast<int32_t>(mFrameBase) + static_cast<int32_t>(mLightFrames[index]);
            SSpriteEngineData* picture = (frame < file->mCount && frame >= 0) ? &file->mSprites[frame] : file->mSprites;
            TakeTexture(draw, picture->mTexture);
        }
        draw.mF08 = CreateShadow(corners, matrix, 1.0f, 1.0f);
        draw.mBlendingMode &= ~kBlendingSemitransparent;
        if (gei_DrawPrimitive->DrawSprite(&draw, -1) == 0) return 0;
        matrix = gShadow.GetNextShadowMatrix();
        ++index;
    }
    return 1;
}

// The same for the grouped path: the shadows go to the primitive being built with AddSprite. The picture is `shadowTexture` (the
// manager's blob shadow) when it is not NULL, else the sprite's; it is worked out once. The record's last field is 0 here.
int32_t SSpriteGameData::AddRegularShadow(CTextureEntry* shadowTexture) {
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagCasts) == 0) return 1;
    if ((flags & kFlagShadows) == 0) return 1;

    SVertex corners[4];
    SSpriteDrawParameters draw{};
    draw.mVertices = corners;
    TakeTexture(draw, shadowTexture != nullptr ? shadowTexture : mSprite->mTexture);
    draw.mF28 = 0;
    AddShadowBlending(draw, static_cast<uint32_t>(mF10));

    CMatrix* matrix = gShadow.GetFirstShadowMatrix();
    while (matrix != nullptr) {
        draw.mF08 = CreateShadow(corners, matrix, 1.0f, 1.0f);
        draw.mBlendingMode &= ~kBlendingSemitransparent;
        if (gei_DrawPrimitive->AddSprite(&draw, -1) == 0) return 0;
        matrix = gShadow.GetNextShadowMatrix();
    }
    return 1;
}
