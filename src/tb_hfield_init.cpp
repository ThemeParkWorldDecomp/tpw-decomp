#include "tb_hfield.h"
#include "m3d2.h"
#include "MapWho.h"
#include "Wavelet.h"
#include "tb_resource_display.h"
#include "ppc_math.h"
#include <cstring>

void CHField::Init(tag_sM3D2Header* header) {
    mF00 = header->mHField;
    mF04 = header->mTextures;
    mGridWidth = mF00->mF18[0] + 1;
    mGridHeight = mF00->mF18[1] + 1;
    mInvCellX = 1.0f / mF00->mF08[2];
    mInvCellZ = 1.0f / mF00->mF08[3];
    const float range = mF00->mF20[1] - mF00->mF20[0];
    mInvHeightRange = range == 0.0f ? 0.0625f : 1.0f / range;
    mF12968 = 0;
    if ((gei_Display.mFlags & 0x10000) != 0) {
        if (mF12970 != nullptr) gFree(mF12970);
        const uint32_t bytes = mF00->mHeightCount * sizeof(float);
        mF12970 = gMalloc(bytes);
        if (mF12970 != nullptr) {
            std::memcpy(mF12970, mF00->mHeights.get(), bytes);
            AdjustHField();
        }
    }
    InitGamut();
}

int32_t CHField::InitGamut() {
    const sHField& field = *mF00;
    gei_Gamut->SetX(field.mF08[0], ppc_fmadds(static_cast<float>(field.mF18[0]), field.mF08[2], field.mF08[0]), field.mF08[2]);
    gei_Gamut->SetZ(field.mF08[1], ppc_fmadds(static_cast<float>(field.mF18[1]), field.mF08[3], field.mF08[1]), field.mF08[3]);
    gei_Gamut->SetYClipper(field.mF20[0], field.mF20[1]);
    gei_Gamut->CalcFurthestDistance();
    gei_MapWho->Init(field.mF18[0], field.mF18[1]);
    return 1;
}
