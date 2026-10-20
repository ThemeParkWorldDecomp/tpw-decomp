#include <cstdint>
#include <cstddef>
#include <cstring>
#include "TPCoaster.h"
#include "TPEngine.h"

// Globals of this unit (no other source names them).
float gBraceBezierMin = 0.0f;                          // 0x1029ce78
float gBraceBezierMax = 1.0f;                          // 0x1029ce7c
const uint32_t* gTrackTextureCacheType = nullptr;      // 0x102b74bc
float gTrackSubdivTimes[100];                          // 0x102b74c0 (up to 0x102b7650)

// 0x10050a3c: gives every control of every brace its neighbours (the previous and next control of the chain, across the braces) and its
// number, then the bezier weight of each control (the distance ratio to the brace's neighbour, clamped for the controls after the pivot).
// The control's Mac word +0x14 (previous control) is the host's mBrace slot; +0x18 is mNext.
void SetupBraceBezierCtrlParams(tag_sCoasterInstance* instance) {
    uint32_t number = 0;
    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        sBraceSpline* spline = brace->mF90;
        for (uint32_t i = 0; i < spline->mCount; ++i) {
            tag_sBezierControl* a = &spline->mControlsA[i];
            tag_sBezierControl* b = &spline->mControlsB[i];
            if (i == 0) {
                tag_sTrackBrace* other = brace->mNext;
                if (other == nullptr) {
                    a->mBrace = nullptr;
                    b->mBrace = nullptr;
                } else {
                    sBraceSpline* os = other->mF90;
                    const int32_t last = static_cast<int32_t>(os->mCount) - 1;
                    a->mBrace = reinterpret_cast<tag_sTrackBrace*>(os->mControlsA + last);
                    b->mBrace = reinterpret_cast<tag_sTrackBrace*>(os->mControlsB + last);
                }
            } else {
                a->mBrace = reinterpret_cast<tag_sTrackBrace*>(&spline->mControlsA[i - 1]);
                b->mBrace = reinterpret_cast<tag_sTrackBrace*>(&spline->mControlsB[i - 1]);
            }
            if (i < spline->mCount - 1U) {
                a->mNext = &spline->mControlsA[i + 1];
                b->mNext = &spline->mControlsB[i + 1];
            } else {
                tag_sTrackBrace* other = brace->mPrev;
                if (other == nullptr) {
                    a->mNext = nullptr;
                    b->mNext = nullptr;
                } else {
                    a->mNext = other->mF90->mControlsA;
                    b->mNext = other->mF90->mControlsB;
                }
            }
            std::memcpy(a->mF00, &number, 4);
            std::memcpy(b->mF00, &number, 4);
            ++number;
        }
    }

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        uint32_t pivot;
        if ((brace->mData->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0)
            pivot = *brace->mF74;
        else
            pivot = 0;
        s3DPoint* here = reinterpret_cast<s3DPoint*>(&brace->mX4C);
        s3DPoint* before = brace->mNext == nullptr ? nullptr : reinterpret_cast<s3DPoint*>(&brace->mNext->mX4C);
        s3DPoint* after = brace->mPrev == nullptr ? nullptr : reinterpret_cast<s3DPoint*>(&brace->mPrev->mX4C);
        tag_sBezierControl* a = brace->mF90->mControlsA;
        tag_sBezierControl* b = brace->mF90->mControlsB;
        for (uint32_t i = 0; i < brace->mF90->mCount; ++i) {
            float v;
            if (i == pivot) {
                v = 0.0f;
            } else if (i < pivot) {
                if (before == nullptr) {
                    v = 0.0f;
                } else {
                    double d1 = static_cast<double>(DistanceBetweenPoints(here, before));
                    double d2 = static_cast<double>(DistanceBetweenPoints(here, &a->mF40));
                    v = static_cast<float>(-d2 / d1);
                }
            } else if (after == nullptr) {
                v = 0.0f;
            } else {
                double d1 = static_cast<double>(DistanceBetweenPoints(here, after));
                double d2 = static_cast<double>(DistanceBetweenPoints(here, &a->mF40));
                v = static_cast<float>(d2 / d1);
                // max with the lower bound, then min with the upper bound, in the original's compare order
                // (a NaN passes both: the compares are "below" and "above")
                if (v < gBraceBezierMin) v = gBraceBezierMin;
                if (v > gBraceBezierMax) v = gBraceBezierMax;
            }
            a->mF0C = v;
            ++a;
            b->mF0C = v;
            ++b;
        }
    }
}

// 0x10050fcc: regenerates the whole track of the instance: the brace tangents and bezier parameters, the samples, the g forces, the bounds
// and collisions, then the extrusions of each pair of braces along the chain (+0x64) into a fresh mesh set.
void GenerateTrack(tag_sCoasterInstance* instance) {
    gCoasterCurrent = instance;
    const uint32_t* type = instance->mType;
    if (type != gTrackTextureCacheType) {
        for (uint32_t i = 0; i < reinterpret_cast<const tag_sCoasterType*>(type)->mTextures->mCount; ++i) {
            const sDTextureSet* set = reinterpret_cast<const tag_sCoasterType*>(type)->mTextures;
            uint32_t flag = (set->mEntries[i].mFlags & 4) != 0 ? 1 : 0;
            *reinterpret_cast<uint32_t*>(static_cast<char*>(gExtrudeFaceStoreB) + i * 0x1c) = flag;
        }
        gTrackTextureCacheType = type;
    }
    RecalcBraceTangents();
    SetupBraceBezierCtrlParams(instance);
    GetActualBraceParams();
    LinearSampleTrack(gCoasterCurrent);
    CalculateTrackGForcesSpeedAndExcitement(gCoasterCurrent);
    RecalcTrackBounds(gCoasterCurrent);
    DoCollisionChecking(gCoasterCurrent);
    tag_sCoasterInstance* owner = gCoasterCurrent;
    while (tag_sDMeshSet* set = owner->mF38) {
        owner->mF38 = set->mF38;
        DestroyDynamicMeshSetInstance(set, nullptr, 0);
    }
    GenerateExtrusions();

    auto blockOf = []() {
        return reinterpret_cast<tag_sCoasterInstance*>(static_cast<char*>(gCoasterCurrentData) - offsetof(tag_sCoasterInstance, mBlock3C));
    };
    tag_sCoasterInstance* blk = blockOf();
    tag_sTrackBrace* cur = blk->mLastBrace;
    tag_sTrackBrace* next = nullptr;
    if (cur != nullptr) next = cur->mPrev;
    bool mode = true;
    int32_t force = 0;   // the original leaves r23 unset until a brace with flag 0x20000
    gExtrudeParamsTable = gExtrudeParamsBuf;
    gExtrudeTOffset = 0.0f;
    gExtrudeWinding = 0;
    blk->mF70 = 0.0f;
    gExtrudeKindFlag = 0;
    blk = blockOf();
    blk->mF74 = 0.0f;
    blk->mF78 = 0.0f;
    blk->mF90 = 0.0f;
    blk->mF8C = 0.0f;
    blk->mF88 = 0.0f;
    blk->mF84 = 0.0f;
    blk->mF80 = 0.0f;
    blk->mF7C = 0.0f;
    const float big = 3.4028234663852886e+38f;
    std::memcpy(blk->mF94, &big, 4);

    while (cur != nullptr && next != nullptr) {
        const uint32_t flags = cur->mFlags;
        if ((flags & 0x10000) != 0) {
            mode = false;
        } else if ((flags & 0x20000) != 0) {
            mode = true;
            force = 1;
        }
        const uint32_t count = cur->mF90->mCount;
        if (count > 1) {
            if (mode) {
                int32_t n = GenerateTrackSubdivisions(cur, cur, gExtrudeParamsTable, gTrackSubdivTimes, static_cast<float>(count - 1));
                ExtrudeTrackSection(cur, cur, n, gTrackSubdivTimes, force);
                force = 0;
            } else {
                blockOf()->mF70 = cur->mF90->mControlsA[count - 1].mF08;
            }
            gExtrudeTOffset = gExtrudeTOffset + static_cast<float>(cur->mF90->mCount - 1);
        }
        if (mode) {
            int32_t n = GenerateTrackSubdivisions(cur, next, gExtrudeParamsTable, gTrackSubdivTimes, 1.0f);
            ExtrudeTrackSection(cur, next, n, gTrackSubdivTimes, force);
            force = 0;
        } else {
            blockOf()->mF70 = next->mF90->mControlsA[0].mF08;
        }
        gExtrudeTOffset = gExtrudeTOffset + 1.0f;
        cur = next;
        next = next->mPrev;
    }
    GetNewTrackMeshSet(1);
    gCoasterCurrent->mFlags &= ~0x4000u;
}
