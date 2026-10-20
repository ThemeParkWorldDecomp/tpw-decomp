// Unit v5752: the ride base members of CMesh (engine 0x1001d97c, 0x1001e644, 0x1001e72c).
#include "tb_mesh.h"
#include <cstring>
#include <new>
#include "Wavelet.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_textureentry.h"
#include "tb_texturecache.h"
#include "ppc_math.h"
#include "platform/msl.h"

namespace {

// Engine data 0x10051ce0: the overlay format (bytes a pixel) for each texture format index.
const int32_t kOverlayFormat[6] = {2, 2, 2, 4, 2, 4};

}  // namespace

// 0x1001d97c. Both branches of the flag test compute the same thing in the original.
void CMesh::CenterMatrix(int32_t flags, float scale) {
    (void)flags;
    const float x0 = mHeader->mBox.mMin[0], y0 = mHeader->mBox.mMin[1], z0 = mHeader->mBox.mMin[2];
    const float x1 = mHeader->mBox.mMax[0], y1 = mHeader->mBox.mMax[1], z1 = mHeader->mBox.mMax[2];
    float sx = scale * (2.0f / (x1 - x0));
    float sy = scale * (2.0f / (y1 - y0));
    float sz = scale * (1.0f / (z1 - z0));
    float tx = ppc_fnmsubs(x1, sx, 1.0f);
    float ty = ppc_fnmsubs(y1, sy, 1.0f);
    float tz = ppc_fnmsubs(z1, sz, 1.0f);
    CMatrix matrix;
    std::memset(&matrix, 0, sizeof(sMatrix));
    matrix.m[3][3] = 1.0f;
    matrix.m[0][0] = sx;
    matrix.m[1][1] = sy;
    matrix.m[2][2] = sz;
    matrix.m[3][0] = tx;
    matrix.m[3][1] = ty;
    matrix.m[3][2] = tz;
    SetLocalMatrix(1, matrix);
}

// 0x1001e644. Answers what CreateProcTexture answers.
int32_t CMesh::GenerateRideBase(CTextureEntry* target, CTextureEntry* source, int32_t x, int32_t y, int32_t flags) {
    sTileOverlay overlay;
    overlay.mData = source->GetCorrectedData();
    overlay.mWidth = source->mWidth;
    overlay.mHeight = source->mHeight;
    overlay.mFormat = kOverlayFormat[gei_TextureSettings.mFormat];
    overlay.mFlags = static_cast<uint32_t>(flags);
    return gei_HField->mProcTextures->CreateProcTexture(target, x, y, &overlay);
}

// 0x1001e72c. The texture records of the cells x0..x1, y0..y1 of the height field that have both
// overlay flags (0x80, 0x200) get a generated texture; the old ones are remembered.
int32_t CMesh::GenerateRideBase(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (!(mRideBaseOld == nullptr && mRideBaseTextures == nullptr && (mFlags & kInitialised) != 0))
        msl_assertion_failed("(mps_OldRideBases==NULL) && (mps_NewRideBases==NULL) && (md_Flags&CMESHFLAGS__INTERNAL__INITIALIZED)",
                             "tb_mesh_exttexture.cpp", 0x44);
    mRideBaseCount = 0;
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            sHFieldQuad& cell = gei_HField->mF00->mQuads[static_cast<int32_t>(
                static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * gei_HField->mF00->mF18[0])];
            if ((cell.mF00 & 0x80) != 0 && (cell.mF00 & 0x200) != 0) ++mRideBaseCount;
        }
    }
    mRideBaseOld = new (std::nothrow) SOldCsTexture[mRideBaseCount];
    mRideBaseTextures = new CsTexture[mRideBaseCount];
    if (mRideBaseOld == nullptr && mRideBaseTextures == nullptr) {
        DeleteRideBase();
        gei_ErrorHandler.Push(2, static_cast<int32_t>(0xB0754000u));
        return 0;
    }
    int32_t ok = 1;
    int32_t k = 0;
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            sHFieldQuad& cell = gei_HField->mF00->mQuads[static_cast<int32_t>(
                static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * gei_HField->mF00->mF18[0])];
            uint32_t flags = cell.mF00;
            if ((flags & 0x80) != 0 && (flags & 0x200) != 0) {
                Ptr32<CsTexture>* where = &mHeader->mTextures[cell.mF02].mTexture;
                CTextureEntry** entryAddr = where->get()->GetTextureEntryAddr(0);
                CTextureEntry* created = new (std::nothrow) CTextureEntry[1];
                if (created == nullptr) {
                    gei_ErrorHandler.Push(2, static_cast<int32_t>(0xB0754000u));
                    for (int32_t i = k - 1; i >= 0; --i) mRideBaseTextures[i].Close();
                    delete[] mRideBaseOld;
                    delete[] mRideBaseTextures;
                    return 0;
                }
                CTextureEntry* source = *entryAddr;
                sTileOverlay overlay;
                overlay.mData = source->GetCorrectedData();
                overlay.mWidth = source->mWidth;
                overlay.mHeight = source->mHeight;
                overlay.mFormat = kOverlayFormat[gei_TextureSettings.mFormat];
                overlay.mFlags = flags;
                ok &= gei_HField->mProcTextures->CreateProcTexture(created, x, y, &overlay);
                gei_TCache->Remove(*entryAddr);
                (mRideBaseTextures + k)->Load(created);
                mRideBaseOld[k].mWhere = where;
                mRideBaseOld[k].mOld = where->get();
                *where = mRideBaseTextures + k;
                ++k;
            }
        }
    }
    if (k != mRideBaseCount) msl_assertion_failed("k==md_RideBaseCount", "tb_mesh_exttexture.cpp", 0x8f);
    if (ok != 0) mFlags |= kRideBase;
    return ok;
}
