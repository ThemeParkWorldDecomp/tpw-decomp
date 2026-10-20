// tb_starfield_frame.cpp: the starfield's members (engine library of SimTheme Park 2.04b3;
// the inventory's tb_starfield.cpp range, 0x1003206c..0x10032200). The stars are points on
// a sphere around the camera, each drawn as a small triangle with the starfield's texture.
#include "Wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_starfield.h"
#include "tb_textureentry.h"
#include "tb_texturecache.h"

namespace {
// The room a polygon's work array needs: its vertices plus what the clipper's six planes can add.
constexpr size_t kWorkVertices = 16;
}  // namespace

CStarfield gStarfield;

CStarfield::~CStarfield() { Close(); }

int32_t CStarfield::Close() {
    if (mTexture != nullptr) {
        mTexture->Close();
        // (mTexture is left pointing at the freed array, as in the original.)
        delete[] mTexture;
    }
    const bool hadStars = mStars != nullptr;
    if (hadStars) {
        gFree(reinterpret_cast<void*&>(mStars));
        mF04 = 0;
        mCount = 0;
    }
    return hadStars ? 1 : 0;
}

int32_t CStarfield::Prepare() {
    gei_TCache->Prepare(mTexture, 0.5f);
    return 1;
}

int32_t CStarfield::Execute() {
    // The original reads the texture's flags without looking for NULL (harmless on the
    // Mac, where address 8 is readable low memory) and has the NULL case below; here a
    // missing texture is simply one that is not on a page.
    void* page = mTexture != nullptr ? mTexture->GetTexture() : nullptr;
    const float size = mSize;
    const float aspect = size * gei_Display.mF04;
    CMatrix transform = mMatrix != nullptr ? (*mMatrix) * (*gei_FinalMatrix) : *gei_FinalMatrix;

    int32_t blend;
    float u0, v0, u1, v1;
    if (mTexture != nullptr) {
        blend = static_cast<int32_t>(mTexture->GetBlendingMode());
        u0 = mTexture->mUOffset;
        u1 = u0 + mTexture->mUSize;
        v0 = mTexture->mVOffset;
        v1 = v0 + mTexture->mVSize;
    } else {
        blend = 0x1256;
        u0 = v0 = u1 = v1 = 0.0f;
    }
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(2, 4, 0x1c4, blend, page);
    if (primitive == nullptr) return 1;
    primitive->mFlags = SPrimitive::kProject;
    const float middle = 0.5f * (u0 + u1);
    for (int32_t i = 0; i < mCount; ++i) {
        float screen[4];
        transform.TransformVertex(screen, &mStars[i * 3], 0);
        if (screen[2] > 0.0f) {
            // (The original leaves the specular words unwritten; they are zero here.)
            // The clipper writes a clipped triangle back here with up to nine vertices (three plus six planes).
            SFVF_TLCTEX vertices[kWorkVertices] = {};
            vertices[0].x = screen[0];
            vertices[0].y = screen[1] + size;
            vertices[0].z = screen[2];
            vertices[0].rhw = screen[3];
            vertices[0].colour = 0xffffffff;
            vertices[0].u = middle;
            vertices[0].v = v0;
            vertices[1].x = screen[0] + aspect;
            vertices[1].y = screen[1] - size;
            vertices[1].z = screen[2];
            vertices[1].rhw = screen[3];
            vertices[1].colour = 0xffffffff;
            vertices[1].u = u1;
            vertices[1].v = v1;
            vertices[2].x = screen[0] - aspect;
            vertices[2].y = screen[1] - size;
            vertices[2].z = screen[2];
            vertices[2].rhw = screen[3];
            vertices[2].colour = 0xffffffff;
            vertices[2].u = u0;
            vertices[2].v = v1;
            if (gei_DrawPrimitive->AddPoly_TLCTEX(vertices, 3, 0x20, nullptr, 0) == 0) return 0;
        }
    }
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(0.0f);
}
