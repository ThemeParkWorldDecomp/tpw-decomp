// RidePreview.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "CBalanceTable.h"
#include "Clock.h"
#include "Debug.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "m3d2.h"
#include "Main.h"
#include "MainWindowCallback.h"
#include "MapWho.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "World.h"
#include <cmath>
#include <cstdio>
#include <cstring>


// Unit v5213: the ride preview's creation (CreateRidePreview 0x1006c3bc) and the scale it fits the ride to (DetermineRidePreviewScale 0x1006c198).

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

namespace {

// Every face set of every node of the header whose texture word is `from` gets `to`.
void ReplaceTextures(tag_sM3D2Header* header, uint32_t from, uint32_t to) {
    tag_sNode* node = header->mNodes.get();
    for (uint32_t n = header->mNodeCount; n != 0; --n) {
        uint32_t* word = reinterpret_cast<uint32_t*>(node->mFaceSets.get());
        for (uint32_t i = node->mFaceSetCount; i != 0; --i) {
            if (*word == from) *word = to;
            word += 4;
        }
        node = reinterpret_cast<tag_sNode*>(reinterpret_cast<char*>(node) + 0xa0);
    }
}

}  // namespace

void TickRidePreview(sRidePreviewInfo* info) {
    if (info->mActive != 0) {
        info->mF70 = static_cast<uint32_t>(gClock.Read());
        info->mActive = 0;
    }
    int32_t now = gClock.Read();
    int32_t elapsed = now - static_cast<int32_t>(info->mF70);
    info->mF70 = static_cast<uint32_t>(now);

    tag_sNode* root = gRideAnimSets[info->mRide]->mHeads->mHeader->mF78.get();
    sMatrix* matrix = reinterpret_cast<sMatrix*>(reinterpret_cast<char*>(root) + 0x10);
    std::memcpy(matrix, info->mF08, 16 * sizeof(float));

    const uint32_t mask = gTPGlobals.mSinTableMask;
    const float scale = gTPGlobals.mSinTableScale;
    const float* table = gTPGlobals.mSinTable;
    const float halfPi = 1.5707963705062866f;
    const float quarterPi = 0.7853981852531433f;
    float angle = info->mAngle;

    auto index = [&](float v) { return mask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(v))); };

    {
        float a = halfPi + angle;
        float b = angle * scale;
        float c = a * scale;
        YRotateLMatrix(matrix, table[index(c)], table[index(b)]);
    }
    {
        float b = -quarterPi * scale;
        float a = quarterPi * scale;
        XRotateLMatrix(matrix, table[index(a)], table[index(b)]);
    }

    float angle2 = info->mAngle;
    float cosA = table[index((halfPi + angle2) * scale)];
    float sinA = table[index(angle2 * scale)];
    float nF60 = -info->mF60;
    float nF5C = -info->mF5C;
    float t4 = ppc_fmadds(info->mW, 0.5f, info->mX);
    float t3 = info->mF6C * 0.5f;
    float t1 = nF60 * sinA;
    float t12 = ppc_fmadds(nF5C, cosA, t1);
    float f9 = ppc_fneg(nF5C);
    float t12b = ppc_fmadds(0.75f, t12, t4);
    float t8 = nF60 * cosA;
    float t4b = 0.699999988079071f * t3;
    float t2 = ppc_fmadds(info->mH, 0.5f, info->mY);
    float* m = reinterpret_cast<float*>(matrix);
    float f1 = m[12] + t12b;
    float t2b = ppc_fnmsubs(t4b, info->mF64, t2);
    float t13 = ppc_fmadds(f9, sinA, t8);
    m[12] = f1;
    t13 = ppc_fmadds(0.699999988079071f, t13, t2b);
    m[13] = m[13] + t13;

    const float tenth = 0.10000000149011612f;
    m[2] = m[2] * tenth;
    m[6] = m[6] * tenth;
    m[10] = m[10] * tenth;
    m[14] = m[14] * tenth;
    m[0] = m[0] * 0.75f;
    m[4] = m[4] * 0.75f;
    m[8] = m[8] * 0.75f;

    float seconds = static_cast<float>(static_cast<double>(static_cast<uint32_t>(elapsed)));
    info->mAngle = ppc_fmadds(0.00039999998989515007f, seconds, info->mAngle);
}
void DestroyRidePreview(sRidePreviewInfo* info) {
    if (info == nullptr) return;
    if (info->mRideType != 0 && gRideTypes[info->mRideType]->mFD0 != nullptr) {
        tag_sM3D2Header* header = gRideAnimSets[info->mRide]->mHeads->mHeader;
        ReplaceTextures(header, info->mTextures[2].mOffset, info->mTextures[0].mOffset);   // 0x7c -> 0x74
        ReplaceTextures(header, info->mTextures[3].mOffset, info->mTextures[1].mOffset);   // 0x80 -> 0x78
        info->mRideType = 0;
    }
    DestroyRideInstance(info->mRide);
}

void HighlightRide() {
    if (gHighlightedRide != nullptr && gPotentialHighlightedRide != nullptr && gHighlightedRide != gPotentialHighlightedRide) {
        UnhighlightRide();
    }
    tag_sRideAnimSet* ride = gPotentialHighlightedRide;
    if (ride == nullptr || (ride->mFlags & 0x20000) != 0 || gRideHighlightDisabled != 0) return;
    tag_sM3D2Header* header = ride->mHeads->mHeader;
    uintptr_t colours = reinterpret_cast<uintptr_t>(gHighlightFaceColours);
    tag_sNode* node = header->mNodes.get();
    for (uint32_t n = 0; n < header->mNodeCount; ++n) {
        if ((node->mFlags & 0x4000) == 0) {
            node->mFlags |= 0x1000000;
            uintptr_t offset = static_cast<uintptr_t>(node->mF5E) * 0xc;
            uintptr_t dest = colours + offset;
            uintptr_t src = reinterpret_cast<uintptr_t>(node->mPoints.get()) + offset;
            if (dest != 0 && src != 0 && node->mF5C != 0) {
                std::memcpy(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), static_cast<size_t>(node->mF5C) * 0xc);
            }
            static_cast<uint32_t*>(gHighlightNodeColours)[n] = node->mPoints.mOffset;
            node->mPoints.mOffset = colours != 0 ? static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(colours))) : 0;
            colours += (static_cast<uintptr_t>(node->mF5E) + node->mF5C) * 0xc;
        }
        node = reinterpret_cast<tag_sNode*>(reinterpret_cast<char*>(node) + 0xa0);
    }
    gPotentialHighlightedRide->mFlags |= 0x20000;
    gPotentialHighlightedRide->mHeads->mHeader->mFlags |= 0x400000;
    gHighlightedRide = gPotentialHighlightedRide;
}

// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: SetPotentiallyHighlightedRide (0x1006cfa4, 12 bytes).

// The ride CalculatePointerWorldPosn found under the pointer this frame.
void SetPotentiallyHighlightedRide(tag_sRideAnimSet* ride) { gPotentialHighlightedRide = ride; }

// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit w133: TickRideHighlights (0x1006cff8, 212 bytes) and TickFlashObjects
// (0x1006d5fc, 244 bytes), which write colour words into the points of ride meshes. Both read a node's points as an array of 32-bit
// colour words (the node's pointer at +0x64, which tag_sNode types as s3DPoint*, read here as uint32_t*).

float gFlashPhase = 0.0f;                    // 0x102d1efc
tag_sRideAnimSet* gFlashObjects = nullptr;   // 0x102d1f00

// While a ride is highlighted: every node of its mesh that does not have flag 0x4000 gets white (0xffffffff) in every one of its colour
// words. The original also reads the absolute time and divides it by 1000 (a helper, 0x10237d2c) and drops the answer; the call is kept.
void TickRideHighlights() {
    if (gHighlightedRide == nullptr) return;
    tag_sM3D2Header* header = gHighlightedRide->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    static_cast<void>(SamsUtilities::UTimer::GetAbsolute());
    for (uint32_t n = 0; n < header->mNodeCount; ++n) {
        for (uint32_t i = 0; i < node->mF5E; ++i) {
            if ((node->mFlags & 0x4000) == 0) reinterpret_cast<uint32_t*>(node->mPoints.get())[i] = 0xffffffffu;
        }
        ++node;
    }
}

// 0x1006d0f8: ends the highlight of the ride gHighlightedRide (only when it has flag 0x20000): every node that is not flagged 0x4000
// gets its flash flag (0x1000000) cleared unless the ride is being flashed (0x4000000), and its points pointer is restored from the
// table of saved colours. Then the ride's flag 0x20000 and the header's 0x400000 are cleared and the global is emptied.
void UnhighlightRide() {
    if (gHighlightedRide == nullptr) return;
    if ((gHighlightedRide->mFlags & 0x20000) == 0) return;
    tag_sM3D2Header* header = gHighlightedRide->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    for (uint32_t i = 0; i < header->mNodeCount; ++i) {
        if ((node->mFlags & 0x4000) == 0) {
            if ((gHighlightedRide->mFlags & 0x4000000) == 0) node->mFlags &= 0xfeffffffu;
            node->mPoints.mOffset = static_cast<const uint32_t*>(gHighlightNodeColours)[i];
        }
        ++node;
    }
    gHighlightedRide->mFlags &= 0xfffdffffu;
    gHighlightedRide->mHeads->mHeader->mFlags &= 0xffbfffffu;
    gHighlightedRide = nullptr;
}

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

void* gHighlightFaceColours;
void* gHighlightNodeColours;
// 0x1006d1d0 (104 bytes, Debug.cpp): 12 bytes for each face (gMaxMeshFaces4 + gMaxMeshF3E), 4 for each node.
void CreateHighlightColourBuffers() {
    gHighlightFaceColours = MReleaseAllocF((gMaxMeshFaces4 + gMaxMeshF3E) * 0xc, 1);
    gHighlightNodeColours = MReleaseAllocF(gMaxMeshNodes << 2, 1);
}

// 0x1006d26c (88 bytes, Debug.cpp): both are freed before either is cleared.
void DestroyHighlightColourBuffers() {
    MReleaseFreeF(gHighlightFaceColours);
    MReleaseFreeF(gHighlightNodeColours);
    gHighlightFaceColours = nullptr;
    gHighlightNodeColours = nullptr;
}

// 0x1006d2f8: pushes the set on the list of flashing sets and flags it. A set that still shares its points with the original
// header gets its own copy of the point block (header flag 0x100000) and every node points into it.
void StartFlashingObject(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    set->mFlags |= 0x4000000u;
    header->mFlags |= 0x800000u;
    set->mFD0 = gFlashObjects;
    gFlashObjects = set;
    tag_sM3D2Header* original = header->mSelf.get();
    if (header->mPoints.mOffset == original->mPoints.mOffset) {
        header->mFlags |= 0x100000u;
        void* block = MReleaseAllocF(static_cast<uint32_t>(header->mPointCount) * 12u, 1);
        header->mPoints = static_cast<s3DPoint*>(block);
        tag_sNode* node = header->mNodes.get();
        uint32_t cursor = header->mPoints.mOffset;
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mPoints.mOffset = cursor;
            node->mFlags |= 0x1000000u;
            cursor += (static_cast<uint32_t>(node->mVertexCount) + static_cast<uint32_t>(node->mF5C)) * 12u;
            ++node;
        }
    } else {
        tag_sNode* node = header->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mFlags |= 0x1000000u;
            ++node;
        }
    }
}

// 0x1006d430: takes the set off the list (it must be in it) and gives its nodes back their original points: the copied block is
// freed and the pointers taken from the original header, or else the copied points are overwritten with the original's.
// A highlighted set is unhighlighted first and highlighted again at the end.
void StopFlashingObject(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    tag_sM3D2Header* original = header->mSelf.get();
    set->mFlags &= 0xfbffffffu;
    header->mFlags &= 0xff7fffffu;
    if (set == gFlashObjects) {
        gFlashObjects = set->mFD0;
    } else {
        tag_sRideAnimSet* previous = gFlashObjects;
        while (previous->mFD0 != set) previous = previous->mFD0;
        previous->mFD0 = set->mFD0;
    }
    set->mFD0 = nullptr;
    bool highlighted = (set->mFlags & 0x20000) != 0;
    if (highlighted) UnhighlightRide();
    if ((header->mFlags & 0x100000) != 0) {
        header->mFlags &= 0xffefffffu;
        MReleaseFreeF(header->mPoints.get());
        header->mPoints.mOffset = original->mPoints.mOffset;
        tag_sNode* node = header->mNodes.get();
        tag_sNode* source = original->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mPoints.mOffset = source->mPoints.mOffset;
            node->mFlags &= 0xfeffffffu;
            ++source;
            ++node;
        }
    } else {
        tag_sNode* node = header->mNodes.get();
        tag_sNode* source = original->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            node->mFlags &= 0xfeffffffu;
            if (node->mPoints.get() != nullptr && source->mPoints.get() != nullptr && node->mF5E != 0)
                std::memcpy(node->mPoints.get(), source->mPoints.get(), static_cast<size_t>(node->mF5E) * 12u);
            ++source;
            ++node;
        }
    }
    if (highlighted) HighlightRide();
}

// The flash colour: the phase (folded to 2 - phase above 1) times 255, converted to an unsigned (CodeWarrior's helper), is the red, green
// and blue byte of the word 0xff000000 | b << 16 | b << 8 | 0xff (b is not masked). The phase then advances by 2 * the frame time
// (gTPGlobals.mF4014, one fused multiply-add in single precision) and is taken modulo 2 in double. Every node of every set in the list
// gFlashObjects (linked through mFD0) gets the word in each of its colour words.
void TickFlashObjects() {
    float level = gFlashPhase;
    if (1.0f < level) level = 2.0f - level;
    const uint32_t b = cw_cvt_fp2unsigned(static_cast<double>(255.0f * level));
    gFlashPhase = ppc_fmadds(2.0f, gTPGlobals.mF4014, gFlashPhase);
    gFlashPhase = static_cast<float>(std::fmod(static_cast<double>(gFlashPhase), 2.0));
    for (tag_sRideAnimSet* set = gFlashObjects; set != nullptr; set = set->mFD0) {
        tag_sM3D2Header* header = set->mHeads->mHeader;
        tag_sNode* node = header->mNodes.get();
        for (uint32_t n = header->mNodeCount; n != 0; --n) {
            uint32_t* word = reinterpret_cast<uint32_t*>(node->mPoints.get());
            for (uint32_t i = node->mF5E; i != 0; --i) {
                *word = (b << 16) | 0xff000000u | (b << 8) | 0xff;
                ++word;
            }
            ++node;
        }
    }
}
