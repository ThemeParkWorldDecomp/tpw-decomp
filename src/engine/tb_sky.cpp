// tb_sky.cpp: name proven by __sinit_tb_sky_cpp in the engine library.

#include "MapWho.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "Wavelet.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

CSky gei_Sky; // __sinit_tb_sky_cpp, 0x1002cac8; native static lifetime runs the same constructor.

CSky::CSky() {
    std::memset(mMatrix, 0, sizeof mMatrix);
    mMatrix[0] = mMatrix[5] = mMatrix[10] = mMatrix[15] = 1.0f;
    mWidth = mHeight = 2400;
    mSphereRadius = 24000.0f;
    SetActiveLayers(gei_System.mRenderer == 4 ? 1 : 4);
    mSpherePosition = {0.0f, 300.0f, 0.0f};
    for (int32_t i = 0; i < 4; ++i) {
        mPosition[i] = {0.0f, 0.0f, 0.0f};
        mScrollDirection[i] = {ppc_fmadds(0.0005f, static_cast<float>(i), 0.0008f), 0.0f,
                              ppc_fnmsubs(0.00003f, static_cast<float>(i), 0.00009f)};
        mLayerAngles[i] = (3.1415927410125732f * static_cast<float>(7 - i)) / 112.0f;
    }
    mSkyTexture = mColorTexture = mCylinderTexture = nullptr;
    int out = 0;
    for (int side = 0; side < 2; ++side) {
        const int base = side * 36;
        for (int row = 0; row < 8; ++row) {
            for (int column = 0; column < 3; ++column) {
                const uint16_t a = base + row * 4 + column;
                const uint16_t b = a + 4;
                mCylinderIndices[out++] = a;
                mCylinderIndices[out++] = b + 1;
                mCylinderIndices[out++] = b;
                mCylinderIndices[out++] = a;
                mCylinderIndices[out++] = a + 1;
                mCylinderIndices[out++] = b + 1;
            }
        }
    }
    mAddCount = 0;
    mF2F0C = -49;
    mF2F10 = 56;
}

CSky::~CSky() { Close(); }

int32_t CSky::Load(CTextureEntry** texture, char* path) {
    if (*texture != nullptr) {
        (*texture)->Close();
        delete[] *texture;
        *texture = nullptr;
    }
    *texture = gei_TLoadManager->LoadFile(path);
    if (*texture == nullptr) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
        return 0;
    }
    (*texture)->mFlags = ((*texture)->mFlags & ~0x100u) | 0x30200u;
    return 1;
}

int32_t CSky::LoadAll(char* sky, char* colors, char* cylinder) {
    if (!Load(&mSkyTexture, sky)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    if (!Load(&mColorTexture, colors)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    if (!Load(&mCylinderTexture, cylinder)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    InitColors();
    return 1;
}

int32_t CSky::LoadAdd(void** out, char* path, D3DVECTOR position, float size, int32_t a, int32_t b) {
    if (mAddCount >= 16) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754012u));
        return 0;
    }
    SSkyAdd& add = mAdds[mAddCount++];
    if (!Load(&add.mTexture, path)) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
        *out = nullptr;
        return 0;
    }
    add.mPosition = position;
    add.mF04 = a;
    add.mF08 = b;
    add.mSize = size;
    *out = add.mTexture;
    return 1;
}

void CSky::RemoveAdd(int32_t index) {
    CTextureEntry* texture = mAdds[index].mTexture;
    mAdds[index].mTexture = nullptr;
    if (texture != nullptr) {
        texture->Close();
        delete[] texture;
    }
}

int32_t CSky::RemoveAdd(void* texture) {
    int32_t index = mAddCount;
    // The inclusive search reads the Mac word mAddCount when the array is full.
    while (index >= 0) {
        void* candidate = index == 16 ? reinterpret_cast<void*>(static_cast<uintptr_t>(
                                               static_cast<uint32_t>(mAddCount))) : mAdds[index].mTexture;
        if (candidate == texture) break;
        --index;
    }
    if (index < 0) return 0;
    RemoveAdd(index);
    if (--mAddCount != 0) {
        SSkyAdd saved = mAdds[index];
        mAdds[index] = mAdds[mAddCount];
        mAdds[mAddCount] = saved;
    }
    return 1;
}

int32_t CSky::SkyLoad(char* folder) {
    char path[120];
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky.tga");
    if (!Load(&mSkyTexture, gFileName(path, 1)))
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky_rgb.tga");
    if (!Load(&mColorTexture, gFileName(path, 1))) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    } else {
        InitColors();
        mColorTexture->Close();
        delete[] mColorTexture;
        mColorTexture = nullptr;
    }
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky_cyl.tga");
    if (!Load(&mCylinderTexture, gFileName(path, 1)))
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    return 1;
}

void CSky::SetSpherePos(D3DVECTOR pos) {
    mSpherePosition = pos;
    Init();
}
void CSky::SetSphereRadius(float radius) {
    mSphereRadius = radius;
    Init();
}
void CSky::ResetSpherePos() {
    mSpherePosition = {0.0f, 300.0f, 0.0f};
    Init();
}
void CSky::SetupScrollDirection(int32_t layer, D3DVECTOR direction) {
    mScrollDirection[layer] = direction;
}
void CSky::SetActiveLayers(int32_t layers) {
    if (layers < 0 || layers > 4)
        msl_assertion_failed("(layers>=0) && (layers<=SKYLAYERS)", "tb_sky.cpp", 0x160);
    mActiveLayers = layers;
}
int32_t CSky::Init() {
    const float stepX = static_cast<float>(mWidth) / 15.0f;
    const float stepZ = static_cast<float>(mHeight) / 15.0f;
    const float halfX = static_cast<float>(mWidth) * 0.5f;
    const float halfZ = static_cast<float>(mHeight) * 0.5f;
    for (int32_t row = 0; row < 16; ++row) {
        const float z = ppc_fmsubs(stepZ, static_cast<float>(row), halfZ);
        for (int32_t column = 0; column < 16; ++column) {
            D3DVECTOR& v = mGrid[column * 16 + row];
            const float x = ppc_fmsubs(stepX, static_cast<float>(column), halfX);
            const float length = static_cast<float>(std::sqrt(static_cast<double>(x * x + z * z)));
            v = {x + mSpherePosition.x, ppc_fnmsubs(0.2f, length, mSpherePosition.y), z + mSpherePosition.z};
            const uint32_t color = static_cast<uint32_t>(msl_rand() & 0x7f) * 0x10101u | 0xff808080u;
            for (auto& colors : mColors) colors[column * 16 + row] = color;
        }
    }
    const float radius = halfX * 0.6f;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(radius * radius)));
    const float low = ppc_fnmsubs(0.2f, length, mSpherePosition.y);
    const float high = ppc_fneg(low);
    const float range = low - high;
    const float y[] = {low, ppc_fnmsubs(range, 0.25f, low), ppc_fnmsubs(range, 0.5f, low), high};
    const float scales[] = {1.0f, 0.98f, 0.96f, 0.84f};
    // The second side repeats angle 16 and ends at 32.
    for (int32_t i = 0; i < 18; ++i) {
        const int32_t angleIndex = i < 9 ? i * 2 : (i - 1) * 2;
        const float angle = (3.1415927410125732f * static_cast<float>(angleIndex)) * 0.0625f;
        for (int32_t k = 0; k < 4; ++k) {
            const float co = static_cast<float>(std::cos(static_cast<double>(angle)));
            const float x = k == 0 ? ppc_fmadds(radius, co, mSpherePosition.x)
                                   : ppc_fmadds(scales[k], radius * co, mSpherePosition.x);
            const float si = static_cast<float>(std::sin(static_cast<double>(angle)));
            const float z = k == 0 ? ppc_fmadds(radius, si, mSpherePosition.z)
                                   : ppc_fmadds(scales[k], radius * si, mSpherePosition.z);
            mCylinder[i * 4 + k] = {x, y[k], z};
        }
    }
    mIndexCount = 0;
    // The index scan uses row-major neighbours, although the positions were filled column-first.
    for (int32_t row = 0; row < 15; ++row) {
        for (int32_t column = 0; column < 15; ++column) {
            const int32_t a = row * 16 + column;
            const int32_t b = a + 16;
            if (mGrid[a].y > high && mGrid[a + 1].y > high && mGrid[b + 1].y > high) {
                mIndices[mIndexCount++] = a + 1;
                mIndices[mIndexCount++] = a;
                mIndices[mIndexCount++] = b + 1;
            }
            if (mGrid[a].y > high && mGrid[b].y > high && mGrid[b + 1].y > high) {
                mIndices[mIndexCount++] = a;
                mIndices[mIndexCount++] = b;
                mIndices[mIndexCount++] = b + 1;
            }
        }
    }
    return 1;
}

// Samples the colour texture on a 16 x 16 grid: one colour per grid point, 0xRRGGBBAA.
// Pixel data is the Mac's byte for byte on every host (NOTES, the pixel converter's "byte
// order decision"), so the words the original loads are put together from bytes here.
void CSky::InitColors() {
    if (mColorTexture == nullptr) return;
    const int32_t stepX = mColorTexture->mWidth / 16;
    const int32_t stepY = mColorTexture->mHeight / 16;
    const int32_t startX = stepX / 2;
    const int32_t startY = stepY / 2;
    const auto* data = static_cast<const uint8_t*>(mColorTexture->GetData());
    const int32_t format = gei_TextureSettings.mFormat;
    if (format < 2 || format > 5) return;
    for (int32_t row = 0; row < 16; ++row) {
        for (int32_t column = 0; column < 16; ++column) {
            uint32_t color;
            if (format == 2 || format == 4) {
                const uint16_t pitch = static_cast<uint16_t>(mColorTexture->mPitch);
                const uint8_t* p = data + static_cast<intptr_t>(((startY + row * stepY) * pitch + startX + column * stepX) * 2);
                const uint32_t pixel = static_cast<uint32_t>(p[0]) << 8 | p[1];
                color = ((pixel & 0xf) << 4) | ((pixel & 0xf0) << 8) | ((pixel & 0xf000) << 16) |
                        ((pixel & 0xf00) << 12) | 0x080808ffu;
            } else {
                const uint8_t* p = data + static_cast<intptr_t>(((startY + row * stepY) * mColorTexture->mPitch + startX + column * stepX) * 4);
                color = static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
                        static_cast<uint32_t>(p[2]) << 8 | p[3];
            }
            for (auto& colors : mColors) colors[column * 16 + row] = color;
        }
    }
}

int32_t CSky::Close() {
    for (CTextureEntry** slot : {&mSkyTexture, &mColorTexture, &mCylinderTexture}) {
        if (*slot != nullptr) {
            (*slot)->Close();
            delete[] *slot;
            *slot = nullptr;
        }
    }
    return 1;
}
CTextureEntry* CSky::GetSkyTEntry() { return mSkyTexture; }
D3DVECTOR* CSky::GetPosition() { return mPosition; }

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
