// tb_sky_render.cpp (engine library of SimTheme Park 2.04b3, the per-frame members of
// tb_sky.cpp's CSky): Prepare 0x1002b5b4, RenderAdd 0x1002b808 and Render 0x1002bb04.
//
// Prepare scrolls the layers and has the texture cache place the sky's pictures. Render
// draws up to four sky layers as 16 by 16 grids of vertices, mirrored again below the
// horizon when the display has flag 0x2000000, then the cylinder around the horizon, with
// RenderAdd drawing the added sprites behind (layer mask 2) and in front (mask 1).
//
// Every a*b+c below written with ppc_fmadds is a fused multiply-add in the original.
//
// Quirks kept: the layer's colour table is read transposed against the vertex order (colour
// index column + 16 * row for the vertex at column * 16 + row); the UV constants rotate a
// quarter turn from one layer to the next and carry over into the mirrored pass; RenderAdd
// ORs the sprite flag 4 in once and never clears it for the later sprites; RenderAdd asks a
// null texture for its blending mode when the add record has no override; Render's four UV
// constants are the callee-saved registers f28..f31 when the sky texture is on no page, which
// the original takes from its caller (zero here).
#include "tb_sky.h"

#include <cstdint>

#include "MapWho.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// The vertex format of the sky's primitives (SFVF_TLCTEX, 0x20 bytes): the math unit writes
// the transformed position, the sky the diffuse colour and the texture coordinates.
struct SkyVertex {
    float mX, mY, mZ, mW;   // 0x00
    uint32_t mDiffuse;      // 0x10
    uint32_t mF14;          // 0x14  not written by the sky
    float mU, mV;           // 0x18, 0x1c
};
static_assert(sizeof(SkyVertex) == 0x20, "SkyVertex layout");

// Four corners of an added sprite, 0x10 bytes each; RotateVertex_Y turns their x and y.
struct SVertexLayout {
    float mX, mY, mZ, mW;
};
constexpr int32_t kVertexStride = 0x20;   // engine data 0x10051dbc, the size of SkyVertex
constexpr int32_t kGridColumns = 16;
constexpr int32_t kGridRows = 16;

// Test before each layer (inlined three times): room in the draw primitive's vertex and index
// memory for 256 more vertices after the primitive in use. 32-bit unsigned arithmetic on
// the byte counts, as the original's.
bool HasRoomForLayer(const CDrawPrimitive& draw) {
    const SPrimitive* current = draw.mCurrent;
    uint32_t vertexBytes = static_cast<uint32_t>(draw.VertexSize()) * (static_cast<uint32_t>(current->mVertexCount) + 0x100u);
    uintptr_t vertexEnd = reinterpret_cast<uintptr_t>(current->mVertexData) + vertexBytes;
    if (reinterpret_cast<uintptr_t>(draw.mVertexEnd) < vertexEnd) return false;
    uint32_t indexBytes = static_cast<uint32_t>(current->mIndexCount) * 2u;
    uintptr_t indexEnd = reinterpret_cast<uintptr_t>(current->mIndexData) + indexBytes;
    return reinterpret_cast<uintptr_t>(draw.mIndexEnd) >= indexEnd;
}

// The inlined math unit call of tb_matrix.h (its assertion names line 152): strided vertices
// through a matrix.
void MathUnitTransform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                       int32_t type) {
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(matrix, out, in, outStride, inStride, count, type);
}

// The sky's matrix (0x2ecc) is a CMatrix in the original.
CMatrix& SkyMatrix(CSky& sky) { return *reinterpret_cast<CMatrix*>(sky.mMatrix); }

// CTextureEntry::GetBlendingMode on a possibly null entry, as RenderAdd calls it. Kept out of
// line so the compiler cannot reason from the call that the pointer was not null.
__attribute__((noinline)) uint32_t BlendingModeOf(CTextureEntry* entry) { return entry->GetBlendingMode(); }

}  // namespace

int32_t CSky::Prepare() {
    if (mActiveLayers == 0) return 1;
    if (mSkyTexture == nullptr || mCylinderTexture == nullptr) return 1;
    if (gei_System.mRenderer == 4) return 1;
    if ((gei_Display.mFlags & 0x80000000u) != 0) return 1;
    if ((gei_System.mFlags & 0xc0000000u) != 0) mSkyTexture->Lock();
    gei_TCache->Prepare(mSkyTexture, 0.0f);
    gei_TCache->Prepare(mCylinderTexture, 0.0f);

    // The scroll step: 25 times the weather's frame time, none when that is beyond 25 either way
    // or the system has flag 4.
    float step = 25.0f * gei_Rain->mDeltaTime;
    if (step > 25.0f || step < -25.0f || (gei_System.mFlags & 4) != 0) step = 0.0f;
    for (int32_t i = 0; i < mActiveLayers; ++i) {
        mPosition[i].x = ppc_fmadds(mScrollDirection[i].x, step, mPosition[i].x);
        mPosition[i].z = ppc_fmadds(mScrollDirection[i].z, step, mPosition[i].z);
        // Wrap into the unit square by dropping the whole part (fctiwz saturates).
        if (mPosition[i].x > 1.0f || mPosition[i].x < -1.0f) {
            mPosition[i].x = mPosition[i].x - static_cast<float>(ppc_fctiwz(mPosition[i].x));
        }
        if (mPosition[i].z > 1.0f || mPosition[i].z < -1.0f) {
            mPosition[i].z = mPosition[i].z - static_cast<float>(ppc_fctiwz(mPosition[i].z));
        }
    }
    for (int32_t i = 0; i < mAddCount; ++i) gei_TCache->Prepare(mAdds[i].mTexture, 0.0f);
    return 1;
}

int32_t CSky::RenderAdd(int32_t layers) {
    if ((gei_Display.mFlags & 0x400000) == 0 || (gei_Display.mFlags & 8) != 0) return 1;

    SSpriteDrawParameters sprite;
    SVertexLayout corners[4];
    float transformed[4];
    sprite.mFlags = 1;
    sprite.mVertices = corners;
    sprite.mF08 = -1;
    sprite.mF28 = 1;
    gei_Display.mZ7C = 1.0f;
    for (int32_t i = 0; i < mAddCount; ++i) {
        SSkyAdd& add = mAdds[i];
        // mF08 is the layer mask, mF04 an override of the blending mode.
        if ((static_cast<uint32_t>(add.mF08) & static_cast<uint32_t>(layers)) == 0) continue;
        CTextureEntry* texture = add.mTexture;
        float half = add.mSize * 0.5f;
        if (texture != nullptr) {
            sprite.mTexture = texture->GetTexture();
            sprite.mBlendingMode = BlendingModeOf(texture);   // replaced below
            sprite.mU0 = texture->GetUOffset();
            sprite.mV0 = texture->GetVOffset();
            sprite.mU1 = sprite.mU0 + texture->GetUSize();
            sprite.mV1 = sprite.mV0 + texture->GetVSize();
            if ((texture->mFlags & 0x100000) != 0) sprite.mFlags |= 4;
        } else {
            sprite.mTexture = nullptr;
            sprite.mBlendingMode = 0x1256;
            sprite.mU0 = sprite.mV0 = sprite.mU1 = sprite.mV1 = 0.0f;
        }
        uint32_t blending = static_cast<uint32_t>(add.mF04);
        if (blending == 0) blending = BlendingModeOf(texture);
        sprite.mBlendingMode = blending | 0x1800;

        SkyMatrix(*this).TransformVertex(transformed, &add.mPosition, 0);
        corners[0].mX = -half;
        corners[0].mY = half;
        corners[1].mX = half;
        corners[1].mY = half;
        corners[2].mX = half;
        corners[2].mY = -half;
        corners[3].mX = -half;
        corners[3].mY = -half;
        RotateVertex_Y(reinterpret_cast<SVertex*>(corners), gViewAngle, nullptr);

        // Squash by the display's aspect, then move to the transformed position (the z and w
        // are the transform's).
        float aspect = gei_Display.mF04;
        for (SVertexLayout& corner : corners) corner.mX = corner.mX * aspect;
        for (SVertexLayout& corner : corners) {
            corner.mX = corner.mX + transformed[0];
            corner.mY = corner.mY + transformed[1];
            corner.mZ = transformed[2];
            corner.mW = transformed[3];
        }
        if (gei_DrawPrimitive->DrawSprite(&sprite, -3) == 0) return 0;
    }
    return 1;
}

int32_t CSky::Render() {
    if (mActiveLayers == 0 || mSkyTexture == nullptr || mCylinderTexture == nullptr) return 1;
    if (gei_System.mRenderer == 4) return 1;

    const uint32_t light = static_cast<uint32_t>(gei_LightManager->mF04);
    CTextureBase* skyPage = mSkyTexture->GetTexture();
    bool direct = true;
    if ((gei_Display.mFlags & 8) == 0 && gei_System.mRenderer != 4) direct = false;
    const uint32_t blending = direct ? 0x1256 : 0x1556;

    // The original's f28..f31: the sky texture's V size, U size, V offset and U offset, which
    // rotate a quarter turn per layer below. Unset (the caller's registers) with no page.
    float f28 = 0.0f, f29 = 0.0f, f30 = 0.0f, f31 = 0.0f;
    if (skyPage != nullptr) {
        f31 = mSkyTexture->GetUOffset();
        f30 = mSkyTexture->GetVOffset();
        f29 = mSkyTexture->GetUSize();
        f28 = mSkyTexture->GetVSize();
    }

    // The view's inverse without its translation, through the projection and the screen.
    CMatrix& matrix = SkyMatrix(*this);
    matrix = *gei_InvViewMatrix;
    matrix.m[3][0] = 0.0f;
    matrix.m[3][1] = 0.0f;
    matrix.m[3][2] = 0.0f;
    matrix *= gei_ProjectionMatrix;
    matrix *= gScreenMatrix;
    if (RenderAdd(2) == 0) return 0;

    const int32_t layers = (gei_Display.mFlags & 8) != 0 ? 1 : mActiveLayers;
    D3DVECTOR mirrored[kGridColumns * kGridRows];
    // Pass 0 draws the layers; pass 1, when the display asks for it, the same again with the
    // grid turned over (x and y negated), continuing the UV rotation.
    for (int32_t pass = 0; pass < 2; ++pass) {
        const D3DVECTOR* source = mGrid;
        if (pass == 1) {
            if ((gei_Display.mFlags & 0x2000000) == 0) break;
            for (int32_t i = 0; i < kGridColumns * kGridRows; ++i) {
                mirrored[i].x = -mGrid[i].x;
                mirrored[i].y = -mGrid[i].y;
                mirrored[i].z = mGrid[i].z;
            }
            source = mirrored;
        }
        for (int32_t layer = 0; layer < layers; ++layer) {
            SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(2, 4, 0x1c4, blending | 0x400800, skyPage);
            if (primitive == nullptr) return 1;
            if (!HasRoomForLayer(*gei_DrawPrimitive)) return 1;
            primitive->mFlags = 0x21;
            void* vertexMemory = primitive->mVertexData;
            MathUnitTransform(&matrix, vertexMemory, const_cast<D3DVECTOR*>(source), kVertexStride, 12,
                              kGridColumns * kGridRows, 0);
            SkyVertex* vertices = reinterpret_cast<SkyVertex*>(primitive->mVertexData);

            // The next quarter turn of the UV constants.
            float turned29 = f29;
            float turned31 = f31;
            f29 = -f28;
            f31 = -f30;
            f28 = turned29;
            f30 = turned31;

            // mF2F0C and mF2F10 (-49 and 56 after construction): the colour mask of layer 0 and
            // how much it changes per layer.
            const uint32_t mask = light & (static_cast<uint32_t>(mF2F0C) - static_cast<uint32_t>(layer) *
                                                                              static_cast<uint32_t>(mF2F10));
            const float scale = mLayerAngles[layer];
            for (int32_t column = 0; column < kGridColumns; ++column) {
                for (int32_t row = 0; row < kGridRows; ++row) {
                    SkyVertex& vertex = vertices[column * kGridRows + row];
                    vertex.mDiffuse = mColors[layer][column + kGridColumns * row] & mask;
                    float u = ppc_fmadds(static_cast<float>(row), scale, mPosition[layer].x);
                    float v = ppc_fmadds(static_cast<float>(column), scale, mPosition[layer].z);
                    vertex.mU = ppc_fmadds(f29, u, f31);
                    vertex.mV = ppc_fmadds(f28, v, f30);
                }
            }
            int32_t vertexCount = kGridColumns * kGridRows;
            int32_t indexCount = gei_DrawPrimitive->AddClipPolyList(vertexCount, mIndices, mIndexCount, 2);
            gei_Display.mZ7C = gei_Display.mZ5C;
            primitive->mVertexCount = vertexCount;
            primitive->mIndexCount = indexCount;
            if (gei_DrawPrimitive->ExecuteCurrentPrimitive(1.0e8f) == 0) return 0;
        }
    }

    // The cylinder: 72 vertices, nine columns of four in each of two halves.
    CTextureBase* cylinderPage = mCylinderTexture->GetTexture();
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(2, 4, 0x1c4, 0x1d56, cylinderPage);
    if (primitive == nullptr) return 1;
    primitive->mFlags = 0x21;
    void* vertexMemory = primitive->mVertexData;
    MathUnitTransform(&matrix, vertexMemory, mCylinder, kVertexStride, 12, 72, 0);
    SkyVertex* vertices = reinterpret_cast<SkyVertex*>(primitive->mVertexData);
    // The V coordinate of the four vertices of a column, top to bottom: the bands 0.49, 0.34,
    // 0.17, 0.01, or 0.49, 0.19, 0.01, 0.49 with the display's flag 0x2000000.
    float bands[4] = {0.49f, 0.34f, 0.17f, 0.01f};
    if ((gei_Display.mFlags & 0x2000000) != 0) {
        bands[0] = 0.49f;
        bands[1] = 0.19f;
        bands[2] = 0.01f;
        bands[3] = 0.49f;
    }
    // The first half is shifted half a unit up the picture.
    for (int32_t half = 0; half < 2; ++half) {
        for (int32_t column = 0; column < 9; ++column) {
            float u = static_cast<float>(column * 2) * 0.0625f;
            for (int32_t j = 0; j < 4; ++j) {
                SkyVertex& vertex = vertices[half * 36 + column * 4 + j];
                vertex.mDiffuse = light;
                vertex.mU = u;
                vertex.mV = half == 0 ? 0.5f + bands[j] : bands[j];
            }
        }
    }
    int32_t vertexCount = 72;
    int32_t indexCount = gei_DrawPrimitive->AddClipPolyList(vertexCount, mCylinderIndices, 288, 2);
    gei_Display.mZ7C = gei_Display.mZ5C;
    primitive->mVertexCount = vertexCount;
    primitive->mIndexCount = indexCount;
    if (gei_DrawPrimitive->ExecuteCurrentPrimitive(0.0f) == 0) return 0;
    return RenderAdd(1) != 0 ? 1 : 0;
}
