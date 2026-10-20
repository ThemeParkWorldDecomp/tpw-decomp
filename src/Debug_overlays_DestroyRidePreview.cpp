#include "Debug.h"
#include <cmath>
#include <cstring>
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "MainWindowCallback.h"
#include "LevelSystems.h"
#include "Clock.h"
#include "rsse.h"

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
