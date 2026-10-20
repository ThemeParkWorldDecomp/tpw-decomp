// tb_sprite.cpp, the regular sprite's draw path (engine library of SimTheme Park 2.04b3; the
// name is the one in its assertions, the inventory files the range under tb_starfield.cpp):
// the members of SSpriteGameData that list a sprite for the frame and turn it into a quad.
//
//   0x1002f3d0 SSpriteGameData::Prepare        (648 bytes)
//   0x1002f688 SSpriteGameData::SetupVertices  (844)
//   0x1003042c SSpriteGameData::CalcRegular    (1,472)
//   0x10030a38 SSpriteGameData::DrawRegular    (84)
//   0x10030ac0 SSpriteGameData::AddRegular     (84)
//
// Every frame CSpriteManager::Prepare calls Prepare on each instance of each list. A sprite that
// is on screen is added to the manager (AddSprite, or AddPartialSprite for the text objects'
// pixel blocks) and has the texture cache prepare its texture. CSpriteManager::RenderSprites
// then calls AddRegular on the sprites of one kind (or DrawRegular on each, with display flag
// 0x80): CalcRegular works the quad out in a stack record and four vertices, and DrawSprite or
// AddSprite draws it.
#include <cstring>

#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// SSpriteGameData::mF10 (names invented).
constexpr uint32_t kSpriteHidden = 0x00000001;       // not drawn
constexpr uint32_t kSpriteFlipX = 0x00000010;        // the picture is mirrored left to right
constexpr uint32_t kSpriteFlipY = 0x00000060;        // and top to bottom (either of two bits)
constexpr uint32_t kSpriteRotate = 0x00000080;       // the quad is turned by Angle()
constexpr uint32_t kSpriteHot = 0x00000200;          // may become the hot sprite; also a depth bias in CalcRegular
constexpr uint32_t kSpriteOnTerrain = 0x00000400;    // CalcRegular lifts it by the height field's height at its position
constexpr uint32_t kSpriteTurnToView = 0x00001000;   // turned about its own position by the camera's heading
constexpr uint32_t kSpriteFlipV = 0x00008000;        // the picture's v coordinates are swapped
constexpr uint32_t kSpriteNoClipTest = 0x00040000;   // listed without a test against the view: the position is on the screen
                                                     // already (x and y from -1 to 1; the interface's sprites)
constexpr uint32_t kSpriteOnGround = 0x00080000;     // the quad lies flat: it spreads over x and z at the world position and the
                                                     // math unit transforms it
constexpr uint32_t kSpriteListed = 0x10000000;       // listed this frame

// gei_Display.mFlags: the hot sprite test needs this one (name invented).
constexpr uint32_t kDisplayHotSprites = 0x01000000;
constexpr uint32_t kDisplayFog = 0x2000;

// Prepare: a sprite counts as seen when a square of 20 times its size at its projected position
// reaches into the view; the cache is told the sprite's depth or, without the test, a fixed distance.
constexpr float kReachPerSize = 20.0f;
constexpr float kNoTestDistance = 0.5f;
constexpr float kPartialDistance = 0.1f;

// SetupVertices: a scale of 0.0 means 20 times the sprite's size; the hot sprite test compares
// the squared distance of the cursor from the sprite with the squared size of the quad times this.
constexpr float kHotFactor = 0.0002f;

// CalcRegular: a screen-space sprite's quad is a twentieth of its size (times the aspect in x) and
// its z is lowered by this when above it; a hot sprite is pushed forward by 0.2 / (1 + z).
constexpr float kPanelScale = 0.05f;
constexpr float kPanelDepthBias = 0.00125f;
constexpr float kDepthPush = 0.2f;

// SSpriteDrawParameters::mFlags: the quad's winding is reversed.
constexpr uint32_t kReverseWinding = 0x2;
// The blending bit CalcRegular adds when the quad is transformed by the math unit (a sprite on the ground).
constexpr uint32_t kBlendGround = 0x800;
// A sprite that is not on a texture page, or has no texture: the blending mode CalcRegular gives.
constexpr uint32_t kBlendNoTexture = 0x1256;
// The projection type of the quad's vertices (CTransformBase::Project's last argument): 2 for a screen-space sprite.
constexpr int32_t kProjectPanel = 2;

}  // namespace

// ---- Prepare ---------------------------------------------------------------------------------

// Always answers 1. A hidden sprite (flag 1) loses its "listed" bit. A sprite without engine data
// is left alone. Three kinds of sprite after that: a partial one (its engine data has a pixel
// block) frees its partial textures when the part of the block in use changed, makes them when
// they are absent and joins the manager's partial list with the cache prepared for each at 0.1; a
// screen-space sprite (flag 0x40000) is listed at once with the cache prepared at 0.5; any other is
// projected (CMatrix::TransformVertex into the first four words) and listed, with the cache
// prepared at its depth, only if its depth is in front of the camera and behind w and the square
// of 20 times its size around it reaches into the view (a NaN in any of the six comparisons
// counts as outside); otherwise it loses its "listed" bit.
int32_t SSpriteGameData::Prepare() {
    uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kSpriteHidden) != 0) {
        mF10 = static_cast<int32_t>(flags & ~kSpriteListed);
        return 1;
    }
    SSpriteEngineData* sprite = mSprite;
    if (sprite == nullptr) return 1;

    if (sprite->mData != nullptr) {
        // A partial sprite: the part of the block in use is this instance's used rectangle.
        const int16_t usedWidth = mUsedWidth;
        const int16_t usedHeight = mUsedHeight;
        if (sprite->mTexture != nullptr &&
            (static_cast<int32_t>(sprite->mUsedWidth) != static_cast<int32_t>(usedWidth) ||
             static_cast<int32_t>(sprite->mUsedHeight) != static_cast<int32_t>(usedHeight)) &&
            sprite->mPartCount != 0) {
            // The partial textures: each that the cache placed comes off its page and lets go of its pixels
            // (they belong to the block), and the array is destroyed (the same lines as
            // SSpriteEngineData::RefreshPartialTexture).
            for (int32_t i = 0; i < sprite->mPartCount; ++i) {
                CTextureEntry* entry = &sprite->mTexture[i];
                if ((entry->mFlags & CTextureEntry::kOnPage) != 0) gei_TCache->Remove(entry);
                sprite->mTexture[i].mData = nullptr;
            }
            delete[] sprite->mTexture;
            sprite->mTexture = nullptr;
            sprite->mPartCount = 0;
        }
        sprite->mUsedWidth = static_cast<uint16_t>(usedWidth);
        sprite->mUsedHeight = static_cast<uint16_t>(usedHeight);

        SSpriteEngineData* data = mSprite;
        if (data->mTexture == nullptr) data->GeneratePartialTexture(mUsedX, mUsedY);
        for (int32_t i = 0; i < data->mPartCount; ++i) gei_TCache->Prepare(&data->mTexture[i], kPartialDistance);
        gei_SpriteManager->AddPartialSprite(this);
        return 1;
    }

    if ((flags & kSpriteNoClipTest) != 0) {
        mF10 = static_cast<int32_t>(flags | kSpriteListed);
        gei_TCache->Prepare(sprite->mTexture, kNoTestDistance);
        gei_SpriteManager->AddSprite(this);
        return 1;
    }

    gei_FinalMatrix->TransformVertex(this, &mPosition, 0);
    const float reach = kReachPerSize * mF2C;
    const float w = mScreenW;
    const bool inView = mDepth > 0.0f && mDepth < w && mScreenX - reach < w && mScreenX + reach > -w &&
                        mScreenY - reach < w && mScreenY + reach > -w;
    if (!inView) {
        mF10 = static_cast<int32_t>(static_cast<uint32_t>(mF10) & ~kSpriteListed);
        return 1;
    }
    gei_SpriteManager->AddSprite(this);
    mF10 = static_cast<int32_t>(static_cast<uint32_t>(mF10) | kSpriteListed);
    gei_TCache->Prepare(mSprite->mTexture, mDepth);
    return 1;
}

// ---- SetupVertices ---------------------------------------------------------------------------

// The quad's four vertices are copies of one point (x, y, z, w) on entry. The sprite's image
// has the part in use (mUsedWidth x mUsedHeight of its data) and the corner of the sprite in it
// (mOffsetX, mOffsetY from the position); the left and right of the quad are the offsets of
// those edges in used widths times xScale, the top and bottom likewise in used heights times
// yScale, so a sprite drawn at its natural size is xScale by yScale across its used part. The
// corners are 0 (left, top), 1 (right, top), 2 (right, bottom), 3 (left, bottom).
//
// Flags: 0x10 negates the horizontal offsets and 0x60 (either bit) the vertical ones, each
// toggling the answer. A flat sprite (0x80000) spreads its vertical offsets over z instead of
// y, toggles the answer once more, and lifts the four y by the display's +0x938 (0.5); an
// upright one spreads them over y. Flag 0x80 then turns the quad by Angle() (RotateVertex_Y for
// an upright sprite, with the middle of the quad as centre; RotateVertex_Z for a flat one); flag
// 0x1000 of an upright one turns it again by the camera's heading about the position it came
// with.
//
// The hot sprite: with flag 0x200 and display flag 0x1000000 the sprite whose projected position
// (x / w, y / w) is nearest the cursor (gSpriteHotX, gSpriteHotY) becomes gHotSprite, if the
// squared distance is below the squared size of the quad times 0.0002 and its w is smaller than
// the current hot sprite's (the nearer one). The test runs on the x offsets already added and
// the y ones not yet; its fused multiply-adds follow the listing.
//
// QUIRK: when the image has no used width or height the divisions give infinities or NaNs.
int32_t SSpriteGameData::SetupVertices(SVertex* vertices, float xScale, float yScale) {
    if (0.0f == xScale) xScale = gei_Display.mF04 * (kReachPerSize * mF2C);
    if (0.0f == yScale) yScale = kReachPerSize * mF2C;

    const SSpriteEngineData* sprite = mSprite;
    const int32_t offsetX = sprite->mOffsetX;
    const int32_t offsetY = sprite->mOffsetY;
    const int32_t width = sprite->mWidth;
    const int32_t height = sprite->mHeight;
    const float usedWidth = static_cast<float>(sprite->mUsedWidth);
    const float usedHeight = static_cast<float>(sprite->mUsedHeight);

    // The point the quad grew from, for the second turn.
    const SVertex origin = vertices[0];
    const uint32_t flags = static_cast<uint32_t>(mF10);
    bool reversed = false;

    float left = xScale * (static_cast<float>(offsetX) / usedWidth);
    float right = xScale * (static_cast<float>(offsetX + width) / usedWidth);
    float bottom = yScale * (static_cast<float>(offsetY + height) / usedHeight);
    float top = yScale * (static_cast<float>(offsetY) / usedHeight);
    if ((flags & kSpriteFlipX) != 0) {
        reversed = !reversed;
        left = left * -1.0f;
        right = right * -1.0f;
    }
    if ((flags & kSpriteFlipY) != 0) {
        reversed = !reversed;
        bottom = bottom * -1.0f;
        top = top * -1.0f;
    }

    const float x0 = vertices[0].x + left;
    vertices[0].x = x0;
    vertices[3].x = x0;
    const float x1 = vertices[1].x + right;
    vertices[1].x = x1;
    vertices[2].x = x1;

    if ((flags & kSpriteHot) != 0 && (gei_Display.mFlags & kDisplayHotSprites) != 0) {
        const float acrossX = right - left;
        const float acrossY = bottom - top;
        const float w = vertices[0].w;
        const float inverse = 1.0f / w;
        const float offX = ppc_fnmsubs(vertices[0].x, inverse, gSpriteHotX);
        const float offY = ppc_fnmsubs(vertices[0].y, inverse, gSpriteHotY);
        const float acrossXSquared = acrossX * acrossX;
        const float offXSquared = offX * offX;
        const float sizeSquared = ppc_fmadds(acrossY, acrossY, acrossXSquared);
        const float distanceSquared = ppc_fmadds(offY, offY, offXSquared);
        if (sizeSquared * kHotFactor > distanceSquared) {
            if (gHotSprite == nullptr || w < gHotSprite->mScreenW) gHotSprite = this;
        }
    }

    if ((flags & kSpriteOnGround) == 0) {
        const float y0 = vertices[0].y + top;
        vertices[1].y = y0;
        vertices[0].y = y0;
        const float y2 = vertices[2].y + bottom;
        vertices[3].y = y2;
        vertices[2].y = y2;
        if ((flags & kSpriteRotate) != 0) RotateVertex_Y(vertices, Angle(), nullptr);
        if ((flags & kSpriteTurnToView) != 0) {
            SVertex centre = origin;
            RotateVertex_Y(vertices, gViewAngle, &centre);
        }
    } else {
        reversed = !reversed;
        const float z0 = vertices[0].z + top;
        vertices[1].z = z0;
        vertices[0].z = z0;
        const float z2 = vertices[2].z + bottom;
        vertices[3].z = z2;
        vertices[2].z = z2;
        for (int32_t i = 0; i < 4; ++i) vertices[i].y = vertices[i].y + gei_Display.mF938;
        if ((flags & kSpriteRotate) != 0) RotateVertex_Z(vertices, Angle());
    }
    return reversed ? 1 : 0;
}

// ---- CalcRegular -----------------------------------------------------------------------------

// Works out the record DrawSprite and AddSprite take for one sprite, in `params` (flags, the
// four vertices at params->mVertices, the colour, the blending mode, the page and the picture's
// corners, the projection type). Always answers 1. A sprite that is not listed this frame (flag
// 0x10000000 clear) asserts (tb_sprite.cpp line 0x4e4) and leaves the record alone.
//
// The picture: without a texture the page is none, the blending mode 0x1256 and the four
// coordinates 0.0. With one, the page is the texture's page when the cache placed it (flag
// 0x1000) else none, the blending mode is GetBlendingMode's, (u0, v0) the entry's offsets and
// (u1, v1) those plus its sizes. Entry flag 0x100000 sets bit 4 of the record's flags, which
// the next line clears (a dead store). Flag 0x8000 of the sprite swaps v0 and v1 at the end.
//
// The position. Flag 0x400 lifts the sprite's y (+0x18) by the height field's height under its
// x and z (bilinear between the four samples of the cell it is in: the cell is the position
// less the field's origin times the inverse cell size cut toward zero; the next column and row
// are clamped to 0..width and 0..height, NOT to width - 1, so the last column reads the next
// row's first sample) plus the display's +0x938. Flag 0x40000 (screen space) takes the position
// as it is with y negated, w = 1 and z lowered by 0.00125 when above it, projection type 2,
// and spreads it into a quad of a twentieth of the size (the x scale times the display's
// aspect). Flag 0x80000 (flat) takes the position as it is (w is whatever the record's
// vertices held), spreads it with SetupVertices (scales 0), and has the math unit transform the
// four vertices by the final matrix (strides 0x10, type 0), adding blending bit 0x800. Any
// other sprite takes the projected x, y, depth and w of Prepare and spreads them; with flag
// 0x200 and no fog (display flag 0x2000) all four z move toward the camera by 0.2 / (1 + z)
// of the last corner. The record's colour is the sprite's, and its flags get bit 2 when
// SetupVertices answered 1.
int32_t SSpriteGameData::CalcRegular(SSpriteDrawParameters* params) {
    if ((static_cast<uint32_t>(mF10) & kSpriteListed) == 0) {
        msl_assertion_failed("0", "tb_sprite.cpp", 0x4e4);
        return 1;
    }

    CTextureEntry* texture = mSprite->mTexture;
    if (texture == nullptr) {
        params->mTexture = nullptr;
        params->mBlendingMode = kBlendNoTexture;
        params->mU0 = 0.0f;
        params->mV0 = 0.0f;
        params->mU1 = 0.0f;
        params->mV1 = 0.0f;
    } else {
        params->mTexture = (texture->mFlags & CTextureEntry::kOnPage) != 0 ? texture->mTexture : nullptr;
        params->mBlendingMode = texture->GetBlendingMode();
        params->mU0 = texture->mUOffset;
        params->mV0 = texture->mVOffset;
        params->mU1 = params->mU0 + texture->mUSize;
        params->mV1 = params->mV0 + texture->mVSize;
        if ((texture->mFlags & 0x100000) != 0) params->mFlags |= 4;
    }
    params->mFlags = 0;

    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kSpriteOnTerrain) != 0) {
        const CHField* field = gei_HField;
        const sHField* heightField = field->mF00;
        const int32_t columns = field->mGridWidth;
        const int32_t rows = field->mGridHeight;
        const float x = mPosition.x;
        const float z = mPosition.z;
        const int32_t column = ppc_fctiwz(static_cast<double>(field->mInvCellX * (x - heightField->mF08[0])));
        const int32_t row = ppc_fctiwz(static_cast<double>(field->mInvCellZ * (z - heightField->mF08[1])));
        int32_t nextColumn = column + 1;
        if (nextColumn > columns) {
            nextColumn = columns;
        } else if (nextColumn < 0) {
            nextColumn = 0;
        }
        int32_t nextRow = row + 1;
        if (nextRow > rows) {
            nextRow = rows;
        } else if (nextRow < 0) {
            nextRow = 0;
        }
        const float cellX = ppc_fmadds(static_cast<float>(column), heightField->mF08[2], heightField->mF08[0]);
        const float cellZ = ppc_fmadds(static_cast<float>(row), heightField->mF08[3], heightField->mF08[1]);
        const float* heights = heightField->mHeights;
        const int32_t rowStart = columns * row;
        const float h00 = heights[column + rowStart];
        const float h10 = heights[nextColumn + rowStart];
        const float h01 = heights[column + columns * nextRow];
        const float fx = field->mInvCellX * (x - cellX);
        const float fz = field->mInvCellZ * (z - cellZ);
        const float bias = gei_Display.mF938;
        float height = ppc_fmadds(fx, h10 - h00, h00);
        height = ppc_fmadds(fz, h01 - h00, height);
        mPosition.y = mPosition.y + (bias + height);
    }

    SVertex* vertices = static_cast<SVertex*>(params->mVertices);
    bool reversed;
    if ((flags & kSpriteNoClipTest) != 0) {
        std::memcpy(&vertices[0], &mPosition, 12);
        vertices[0].y = -vertices[0].y;
        vertices[0].w = 1.0f;
        if (vertices[0].z > kPanelDepthBias) vertices[0].z = vertices[0].z - kPanelDepthBias;
        params->mF28 = kProjectPanel;
        vertices[1] = vertices[0];
        vertices[2] = vertices[1];
        vertices[3] = vertices[2];
        const float scale = kPanelScale * mF2C;
        reversed = SetupVertices(vertices, gei_Display.mF04 * scale, scale) != 0;
    } else {
        params->mF28 = 0;
        if ((flags & kSpriteOnGround) != 0) {
            std::memcpy(&vertices[0], &mPosition, 12);
            vertices[1] = vertices[0];
            vertices[2] = vertices[1];
            vertices[3] = vertices[2];
            reversed = SetupVertices(vertices, 0.0f, 0.0f) != 0;
            if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
            gei_MathUnit->Transform(gei_FinalMatrix, vertices, vertices, 0x10, 0x10, 4, 0);
            params->mBlendingMode |= kBlendGround;
        } else {
            std::memcpy(&vertices[0], static_cast<const void*>(this), sizeof(SVertex));
            vertices[1] = vertices[0];
            vertices[2] = vertices[1];
            vertices[3] = vertices[2];
            reversed = SetupVertices(vertices, 0.0f, 0.0f) != 0;
            if ((flags & kSpriteHot) != 0 && (gei_Display.mFlags & kDisplayFog) == 0) {
                const float z = vertices[3].z;
                const float pushed = z - kDepthPush / (1.0f + z);
                vertices[3].z = pushed;
                vertices[2].z = pushed;
                vertices[1].z = pushed;
                vertices[0].z = pushed;
            }
        }
    }

    params->mF08 = mF24;
    if (reversed) params->mFlags |= kReverseWinding;
    if ((flags & kSpriteFlipV) != 0) {
        const float v = params->mV0;
        params->mV0 = params->mV1;
        params->mV1 = v;
    }
    return 1;
}

// ---- DrawRegular and AddRegular ----------------------------------------------------------------

// The record and its four vertices live on the stack of the original (the vertices' unwritten
// words are whatever was there; here zero). Each answers 0 if CalcRegular does (it never does)
// and else what DrawSprite or AddSprite answers (DrawSprite: 0 when the renderer fails, else 1;
// AddSprite: always 1). The mask is -1, every clip plane.
int32_t SSpriteGameData::DrawRegular() {
    SSpriteDrawParameters params = {};
    SVertex vertices[4] = {};
    params.mVertices = vertices;
    if (CalcRegular(&params) == 0) return 0;
    return gei_DrawPrimitive->DrawSprite(&params, -1);
}

int32_t SSpriteGameData::AddRegular() {
    SSpriteDrawParameters params = {};
    SVertex vertices[4] = {};
    params.mVertices = vertices;
    if (CalcRegular(&params) == 0) return 0;
    return gei_DrawPrimitive->AddSprite(&params, -1);
}
