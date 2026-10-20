#include <cstdio>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"

// The hoardings' face set takes the first texture word of the dynamic mesh's header, plus an offset per kind.
static void SetHoardingTexture(tag_sRideAnimSet* set, uint32_t add) {
    tag_sDMeshSet* dm = set->mDynamicMeshSet;
    dm->mEntries->mFaceSets->mTexture.mOffset = dm->mMesh->mHeader->mTextures.mOffset + add;
}

// 0x10061268 (416 bytes).
void SetRideHoardingLevel(tag_sRideAnimSet* set, float level) {
    if ((set->mFlags & 2) == 0) return;
    uint32_t count = *static_cast<uint32_t*>(set->mHoardingMesh);
    sVertexPosnSet* vertices = set->mDynamicMeshSet->mVertexSets;
    sUVSet* uvs = set->mDynamicMeshSet->mMesh->mHeader->mF78.get()->mUVs.get();
    for (int32_t i = 0; i < static_cast<int32_t>(count); i++) {
        float scale = (i & 1) == 0 ? 1.0f : 0.8f;
        float q = static_cast<float>(i) / static_cast<float>(static_cast<int32_t>(count));
        float t = ppc_fnmsubs(4.0f, ppc_fmsubs(0.8f, q, level - 0.25f), 1.0f);
        if (t < 0.0f) t = 0.0f;
        if (1.0f < t) t = 1.0f;
        uint32_t base = static_cast<uint32_t>(i) * 4;
        uint32_t i1 = base + 1, i2 = base + 2, i3 = base + 3;
        float lift = 10.0f * t * scale;
        vertices[base >> 2].y[base & 3] = vertices[i2 >> 2].y[i2 & 3] + lift;
        float v = ppc_fnmsubs(scale, t, 1.0f);
        vertices[i1 >> 2].y[i1 & 3] = vertices[i3 >> 2].y[i3 & 3] + lift;
        uvs[i2 >> 2].mF00[i2 & 3] = 0.0f;
        uvs[i2 >> 2].mF00[4 + (i2 & 3)] = v;
        uvs[i3 >> 2].mF00[i3 & 3] = 1.0f;
        uvs[i3 >> 2].mF00[4 + (i3 & 3)] = v;
    }
}

// 0x10061060 (456 bytes).
void EnableRideHoardings(tag_sRideAnimSet* set, int32_t enable) {
    if ((set->mFlags & 2) == 0) return;
    tag_sDMeshSet* dm = set->mDynamicMeshSet;
    if (enable != 0) {
        tag_sNode* node = set->mHeads->mHeader->mF78.get();
        sMatrix* matrix = dm->mEntries->mMatrix;
        SetMatrixIdentity(matrix);
        matrix->m[3][0] = node->mMatrix.m[3][0];
        matrix->m[3][1] = node->mMatrix.m[3][1];
        matrix->m[3][2] = node->mMatrix.m[3][2];
        float angle = 0.017453292f * static_cast<float>(set->mHeads->mF10);
        float a = (1.5707964f + angle) * gTPGlobals.mSinTableScale;
        float b = angle * gTPGlobals.mSinTableScale;
        int32_t ia = static_cast<int32_t>(a);
        int32_t ib = static_cast<int32_t>(b);
        float c = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ia)];
        float s = gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ib)];
        YRotateLMatrixNoTrans(matrix, c, s);
        tag_sM3D2Header* from = set->mHeads->mHeader;
        tag_sM3D2Header* to = dm->mMesh->mHeader;
        to->mBox = from->mBox;
        DMEnableMeshRender(dm, set->mFC0, set->mFC4);
    } else {
        SetHoardingTexture(set, 0);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x100;
        DMDisableMeshRender(dm, set->mFC0, set->mFC4);
    }
}

// 0x100614a0 (752 bytes).
void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) {
    if (set->mHoardingMesh == nullptr) return;
    if (flag == 1 && ((set->mFlags & 0x80) != 0 || (set->mFlags & 0xf00) == 0)) {
        SetHoardingTexture(set, 0);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x100;
    } else if (flag == 2 && (set->mFlags & 0x200) == 0) {
        SetHoardingTexture(set, 8);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x200;
    } else if (flag == 4 && (set->mFlags & 0x400) == 0) {
        SetHoardingTexture(set, 0x10);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x400;
    } else if (flag == 8 && (set->mFlags & 0x800) == 0) {
        SetHoardingTexture(set, 0x18);
        set->mFlags &= 0xfffff0ffu;
        set->mFlags |= 0x800;
    }
    set->mHoardingSpeed = 0.2f;
    if ((set->mFlags & 0x20) == 0) EnableRideHoardings(set, 1);
    set->mFlags |= 0x60;
    set->mFlags &= ~0x80u;
}

// 0x10061840 (344 bytes).
void ReactivateHoardings(tag_sRideAnimSet* set) {
    if (set->mHoardingMesh == nullptr) return;
    if ((set->mFlags & 0x20) != 0) {
        EnableRideHoardings(set, 1);
        if ((set->mFlags & 0x40) != 0) {
            set->mHoardingSpeed = 0.2f;
        } else if ((set->mFlags & 0x80) != 0) {
            set->mHoardingSpeed = -0.3f;
        }
    }
    SetRideHoardingLevel(set, set->mHoardingLevel);
    uint32_t flags = set->mFlags;
    if ((flags & 0x100) != 0) {
        SetHoardingTexture(set, 0);
    } else if ((flags & 0x200) != 0) {
        SetHoardingTexture(set, 8);
    } else if ((flags & 0x400) != 0) {
        SetHoardingTexture(set, 0x10);
    } else if ((flags & 0x800) != 0) {
        SetHoardingTexture(set, 0x18);
    } else {
        SetHoardingTexture(set, 0);
        set->mFlags |= 0x100;
    }
}
