// Unit v5213: the ride preview's creation (CreateRidePreview 0x1006c3bc) and the scale it fits the ride to (DetermineRidePreviewScale 0x1006c198).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "MainWindowCallback.h"
#include "TPEngine.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"

sRidePreviewInfo gRidePreviewBlock;   // 0x102d1f20

// Fits the ride's box (the height map block's floats at 0x18..0x2c: two corners) into a w by h area. The scale is the smaller of two
// ratios (a NaN keeps the first); offsets and ranges come out through the pointers. The 5th pointer is not used.
void DetermineRidePreviewScale(uint32_t ride, float w, float h, float* scale, float* unused, float* offsetX, float* offsetZ,
                               float* rangeX, float* rangeZ, float* ratio, float* distance, float* distanceMid) {
    static_cast<void>(unused);
    const float* box = static_cast<const float*>(gRideAnimSets[ride]->mHeightMap);
    float cornerA[3] = {box[0x18 / 4], 0.0f, box[0x20 / 4]};          // sp+0x40, the middle word is overwritten with 0
    float centre[3];                                                   // sp+0x4c
    centre[0] = (box[0x18 / 4] + box[0x24 / 4]) * 0.5f;
    centre[1] = 0.0f;
    centre[2] = (box[0x20 / 4] + box[0x2c / 4]) * 0.5f;
    *rangeX = box[0x24 / 4] - box[0x18 / 4];
    *rangeZ = box[0x2c / 4] - box[0x20 / 4];
    const float hz = *rangeZ * 0.5f;
    const float hx = *rangeX * 0.5f;
    const float zz = hz * hz;
    const float half = static_cast<float>(std::sqrt(ppc_fmadd(static_cast<double>(hx), static_cast<double>(hx), static_cast<double>(zz))));
    const s3DPoint* far = reinterpret_cast<const s3DPoint*>(box + 0x24 / 4);
    const float d1 = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(centre), far);
    *distanceMid = d1;
    *distance = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(cornerA), far);
    const float first = ((w / 0.75f) * 0.5f) / half;
    const float second = h / *distance;
    *scale = (first < second) ? first : second;   // bge is taken for an unordered compare: a NaN picks the second
    *offsetX = -((box[0x18 / 4] / 10.0f) * *scale);
    *offsetZ = -((cornerA[1] / 10.0f) * *scale);
    const float dist = *distance;
    const float a = half / dist;
    const float b = d1 / dist;
    *ratio = b / (b + a);
}

// Makes the ride instance the preview shows, finds the sign textures of the ride type's mesh and of the instance's mesh and swaps the
// ride type's for the instance's in the instance's face sets, scales the mesh to fit, and starts the ride's animation 5.
sRidePreviewInfo* CreateRidePreview(float x, float y, float z, float w, float h, uint32_t id) {
    sRidePreviewInfo* preview = &gRidePreviewBlock;
    preview->mRide = InstanceRide(id, 0xc01, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0);
    tag_sRideAnimSet* type = gRideTypes[id];
    tag_sRideAnimSet* set = gRideAnimSets[preview->mRide];
    tag_sM3D2Header* header = set->mHeads->mHeader;
    if (type->mFD0 != nullptr) {
        tag_sM3D2Header* typeHeader = type->mHeads->mHeader;
        sTexture* typeSign1 = nullptr;
        sTexture* typeSign2 = nullptr;
        sTexture* sign1 = nullptr;
        sTexture* sign2 = nullptr;
        sTextureInfo* info = typeHeader->mTextureInfos.get();
        for (uint32_t n = typeHeader->mTextureCount; n != 0; --n, ++info) {
            if (!((info->mFlags & 2) && (info->mFlags & 8)) && NS_MacDoze::_stricmp(info->mName.get(), "sign1.tga") != 0) {
            } else {
                typeSign1 = typeHeader->mTextures.get() + (info - typeHeader->mTextureInfos.get());
            }
            if (!((info->mFlags & 2) && (info->mFlags & 0x10)) && NS_MacDoze::_stricmp(info->mName.get(), "sign2.tga") != 0) {
            } else {
                typeSign2 = typeHeader->mTextures.get() + (info - typeHeader->mTextureInfos.get());
            }
        }
        info = header->mTextureInfos.get();
        for (uint32_t n = header->mTextureCount; n != 0; --n, ++info) {
            if (!((info->mFlags & 2) && (info->mFlags & 8)) && NS_MacDoze::_stricmp(info->mName.get(), "sign1.tga") != 0) {
            } else {
                sign1 = header->mTextures.get() + (info - header->mTextureInfos.get());
            }
            if (!((info->mFlags & 2) && (info->mFlags & 0x10)) && NS_MacDoze::_stricmp(info->mName.get(), "sign2.tga") != 0) {
            } else {
                sign2 = header->mTextures.get() + (info - header->mTextureInfos.get());
            }
        }
        if (typeSign1 != nullptr && typeSign2 != nullptr && sign1 != nullptr && sign2 != nullptr) {
            preview->mRideType = id;
            preview->mTextures[0].set(sign1);
            preview->mTextures[1].set(sign2);
            preview->mTextures[2].set(typeSign1);
            preview->mTextures[3].set(typeSign2);
            // the order of the original's stores: +0x74 sign1, +0x78 sign2, +0x7c typeSign1, +0x80 typeSign2 (with the two searches
            // swapped: the first search ran over the ride type's mesh, and its results sit in r21 and r20)
            tag_sNode* node = header->mNodes.get();
            for (uint32_t n = header->mNodeCount; n != 0; --n, ++node) {
                sFaceSet* faceSet = node->mFaceSets.get();
                for (uint32_t k = node->mFaceSetCount; k != 0; --k, ++faceSet) {
                    if (faceSet->mTexture.get() == sign1) faceSet->mTexture.set(typeSign1);
                }
            }
            node = header->mNodes.get();
            for (uint32_t n = header->mNodeCount; n != 0; --n, ++node) {
                sFaceSet* faceSet = node->mFaceSets.get();
                for (uint32_t k = node->mFaceSetCount; k != 0; --k, ++faceSet) {
                    if (faceSet->mTexture.get() == sign2) faceSet->mTexture.set(typeSign2);
                }
            }
        }
    }
    preview->mX = x;
    preview->mY = y;
    preview->mW = w;
    preview->mH = h;
    const float scale1 = 1.0f;
    float scaleOut;
    float offsetX, offsetZ;
    DetermineRidePreviewScale(preview->mRide, w, h, &scaleOut, nullptr, &offsetX, &offsetZ, &preview->mF5C, &preview->mF60,
                              &preview->mF64, &preview->mF68, &preview->mF6C);
    s3DPoint scaleVector;
    scaleVector.x = scale1 * scaleOut;
    scaleVector.y = scale1 * scaleOut;
    scaleVector.z = scale1 * scaleOut;
    ScaleMesh(gRideAnimSets[preview->mRide]->mHeads, &scaleVector);
    const float half = 0.5f * scaleOut;
    preview->mF5C = preview->mF5C * half;
    preview->mF60 = preview->mF60 * half;
    preview->mF68 = preview->mF68 * scaleOut;
    preview->mF6C = preview->mF6C * scaleOut;
    sMatrix matrix;
    SetMatrixIdentity(&matrix);
    matrix.m[3][2] = z;
    std::memcpy(preview->mF08, reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get()) + 0x10, sizeof(preview->mF08));
    SetRideMobile(preview->mRide);
    tag_sRideAnimSet** slot = &gRideAnimSets[preview->mRide];
    SetMeshInUISpace((*slot)->mHeads);
    (*slot)->mMatrix = &matrix;
    EnableMeshDraw(gRideAnimSets[preview->mRide]->mHeads, &matrix, 1);
    if (preview->mRide != 0) SetCurRideAnim(gRideAnimSets[preview->mRide], 5, 0, 1, 1.0f, 0);
    preview->mActive = 1;
    preview->mAngle = static_cast<float>(std::fmod(static_cast<double>(preview->mAngle), 6.2831854820251465));
    return preview;
}
