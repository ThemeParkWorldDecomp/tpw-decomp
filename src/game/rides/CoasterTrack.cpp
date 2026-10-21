// CoasterTrack.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "analyser.h"
#include "Debug.h"
#include "FS_Online.h"
#include "helpers/CoasterEditHelpers.h"
#include "HFieldTextures.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "Mouse.h"
#include "online.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TrackCell.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

// Unit v51074: small coaster, heightfield and mesh helpers (names of fields invented where the headers had none).

// 0x1005092c: the orientation block starts with zero angles and a maximal word at +0x1c (past the struct: the instance's next word).
void SetInitialOrientationParams(sCoasterOrientation* orient, sCoasterTrackParams* params, int32_t a) {
    (void)params;
    (void)a;
    uint8_t* p = reinterpret_cast<uint8_t*>(orient);
    const float zero = 0.0f;
    const float big = std::numeric_limits<float>::max();
    std::memset(p, 0, 4);
    std::memcpy(p + 0x18, &zero, 4);
    std::memcpy(p + 0x14, &zero, 4);
    std::memcpy(p + 0x10, &zero, 4);
    std::memcpy(p + 0x0c, &zero, 4);
    std::memcpy(p + 0x08, &zero, 4);
    std::memcpy(p + 0x04, &zero, 4);
    std::memcpy(p + 0x1c, &big, 4);
}

// 0x100509bc
void SetInitialCoasterInterpParams(sCoasterTrackParams* params) {
    uint8_t* p = reinterpret_cast<uint8_t*>(params);
    const float zero = 0.0f;
    const float big = std::numeric_limits<float>::max();
    std::memcpy(p + 0x38, &zero, 4);
    std::memset(p + 0x3c, 0, 4);
    std::memcpy(p + 0x54, &zero, 4);
    std::memcpy(p + 0x50, &zero, 4);
    std::memcpy(p + 0x4c, &zero, 4);
    std::memcpy(p + 0x48, &zero, 4);
    std::memcpy(p + 0x44, &zero, 4);
    std::memcpy(p + 0x40, &zero, 4);
    std::memcpy(p + 0x58, &big, 4);
}

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

// TPConfig.cpp (main app): the destroyers of a coaster's track and pylons, and the track position control. Unit v5038.

// 0x10050df4: the chain of mesh sets (linked by the set's +0x38) is taken off the instance one set at a time and destroyed.
void DestroyCoasterTrack(tag_sCoasterInstance* instance) {
    while (tag_sDMeshSet* set = instance->mF38) {
        instance->mF38 = set->mF38;
        DestroyDynamicMeshSetInstance(set, nullptr, 0);
    }
}

// 0x10050e84: every node of the pylon list is destroyed and freed. A node with flag 1 holds one brace and its info (brace + 0x78); the others
// destroy their braces one by one when the control's flag 4 is set, then the 3D object.
void DestroyCoasterPylons(tag_sCoasterInstance* instance) {
    sCoasterPylon* node = instance->mPylons;
    while (node != nullptr) {
        if ((node->mFlags & 1) == 0) {
            if ((node->mControl->mFlags & 4) != 0) {
                for (uint32_t i = 0; i < node->mCount; ++i) DestroyTrackBrace(node->mBraces[i], node->mControl);
            }
            Destroy3DObjectFromAIWorld(node->mF0C, node->mF10);
        } else {
            tag_sTrackBrace* brace = node->mBraces[0];
            DestroyBraceInfo(brace, reinterpret_cast<sBraceInfo*>(reinterpret_cast<char*>(brace) + 0x78));
        }
        sCoasterPylon* next = node->mNext;
        MReleaseFreeF(node);
        node = next;
    }
}

// 0x10050f88
void RecalcTrackParams(tag_sCoasterInstance* instance) {
    (void)instance;
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

// 0x10051300: one control further along the track.
void IncrementTrackPosnControl(sTrackPosnControl* control) {
    tag_sTrackBrace* brace = control->mBrace;
    tag_sTrackBrace* braceB = control->mBraceB;
    if (brace == braceB) {
        if (control->mIndexB < brace->mF90->mCount - 1u) {
            control->mIndex = control->mIndex + 1;
            control->mIndexB = control->mIndexB + 1;
            return;
        }
        control->mBraceB = braceB->mPrev;
        control->mIndex = control->mIndex + 1;
        control->mIndexB = 0;
        return;
    }
    if (braceB->mF90->mCount > 1) {
        control->mBrace = braceB;
        control->mIndex = 0;
        control->mIndexB = control->mIndexB + 1;
        return;
    }
    control->mBrace = braceB;
    control->mBraceB = control->mBraceB->mPrev;
    control->mIndex = control->mBrace->mF90->mCount - 1;
}

// 0x100513fc: one control back.
void DecrementTrackPosnControl(sTrackPosnControl* control) {
    tag_sTrackBrace* brace = control->mBrace;
    if (brace == control->mBraceB) {
        if (control->mIndex != 0) {
            control->mIndex = control->mIndex - 1;
            control->mIndexB = control->mIndexB - 1;
            return;
        }
        control->mBrace = brace->mNext;
        control->mIndex = control->mBrace->mF90->mCount - 1;
        control->mIndexB = 0;
        return;
    }
    if (brace->mF90->mCount > 1) {
        control->mBraceB = brace;
        control->mIndexB = control->mIndex;
        control->mIndex = control->mIndex - 1;
        return;
    }
    control->mBraceB = brace;
    control->mBrace = control->mBrace->mNext;
    control->mIndex = control->mBrace->mF90->mCount - 1;
}

// 0x100514f0: the point at t between the two controls (both read from the spline's first array, +4; the 0x58 stride is the Mac's).
void GetTrackPosition(sTrackPosnControl* control, float t, s3DPoint* point) {
    char* a = reinterpret_cast<char*>(control->mBrace->mF90->mControlsA) + control->mIndex * 0x58;
    char* b = reinterpret_cast<char*>(control->mBraceB->mF90->mControlsA) + control->mIndexB * 0x58;
    BezierPosition(reinterpret_cast<tag_sBezierControl*>(a), reinterpret_cast<tag_sBezierControl*>(b), t, point);
}

// 0x1005158c (the point stays in its register, r6, behind the float's slot r5)
void GetWrappedTrackPosition(sTrackPosnControl* control, tag_sTrackBrace* brace, float t, s3DPoint* point) {
    BezierPosition(control->mBrace->mF90->mControlsA + control->mIndex, brace->mF90->mControlsA, t, point);
}

// 0x1005162c: walks the brace chain with a track position control, summing the distance of each step into *total and
// returning the sum of the sample counts of each step (the control lives on the stack: index, brace, indexB, braceB).
int32_t CalcNumTrackSamples(tag_sTrackBrace* brace, float step, float* total) {
    int32_t count = 0;
    sTrackPosnControl control;
    control.mIndex = 0;
    control.mBrace = brace;
    if (brace->mF90->mCount > 1) {
        control.mBraceB = brace;
        control.mIndexB = 1;
    } else {
        control.mBraceB = brace->mPrev;
        control.mIndexB = 0;
    }
    s3DPoint prev;
    s3DPoint next;
    GetTrackPosition(&control, 0.0f, &prev);
    while (control.mBraceB != nullptr) {
        GetTrackPosition(&control, 1.0f, &next);
        float dist = DistanceBetweenPoints(&prev, &next);
        float ratio = dist / step;
        *total = *total + dist;
        float arg = ppc_fmadds(3.0f, ratio, 1.0f);
        count += FloatToCell(static_cast<double>(arg));
        IncrementTrackPosnControl(&control);
        prev = next;
    }
    return count;
}

namespace {

// The instance whose +0x3c block gCoasterCurrentData points at (the Mac reads *(*data + 0x18), the instance's +0x54).
tag_sCoasterInstance* DataInstance() {
    return reinterpret_cast<tag_sCoasterInstance*>(static_cast<char*>(gCoasterCurrentData) - offsetof(tag_sCoasterInstance, mBlock3C));
}

// Mac words that the header keeps as byte pads: the sample capacity (+0x98) and the widest span (+0xa0).
uint32_t* CapacityWord(tag_sCoasterInstance* instance) { return reinterpret_cast<uint32_t*>(&instance->mF94[4]); }
uint32_t* SpanWord(tag_sCoasterInstance* instance) { return reinterpret_cast<uint32_t*>(instance->mFA0); }

// The Mac word at a control's +0x14 (a control to move to when the sample's t is negative). The host pointer of mBrace covers that
// word, so the host keeps it in the unused pad at +0x50.
tag_sBezierControl* Link14(const tag_sBezierControl* control) {
    tag_sBezierControl* link;
    std::memcpy(&link, &control->mF4C[4], sizeof link);
    return link;
}

tag_sBezierControl* ControlAt(tag_sBezierControl* base, uint32_t index) {
    return reinterpret_cast<tag_sBezierControl*>(reinterpret_cast<char*>(base) + index * 0x58u);
}

tag_sBezierControl* SampleA(const sTrackSample* sample) { return reinterpret_cast<tag_sBezierControl*>(sample->mPiece); }
tag_sBezierControl* SampleB(const sTrackSample* sample) { return sample->mPieceB; }
float& SampleT(sTrackSample* sample) { return *reinterpret_cast<float*>(sample->mF34); }

// The orientation block at the Mac's sp+0x7c: the 0x1c-byte orientation and one more word (set to FLT_MAX) after it.
struct sOrientationBlock {
    sCoasterOrientation mOrient;
    float mExtra;
};

}  // namespace

// 0x10051770: samples the coaster's track at even lengths (instance +0xac): the sample's point, the three angles of the orientation, the two
// controls and the t. A position control walks the braces from the last one back; when it runs out the walk wraps to the first
// control of the brace before the last. Afterwards the widest span of the braces' samples, the count and the entrance samples.
// Reproduced: if the first position control has no second brace the Mac loop runs on registers nothing set; this returns.
void LinearSampleTrack(tag_sCoasterInstance* instance) {
    *SpanWord(instance) = 0;
    tag_sTrackBrace* first = DataInstance()->mLastBrace;
    if (first == nullptr) return;

    sTrackPosnControl control;
    control.mIndex = 0;
    control.mBrace = first;
    if (first->mF90->mCount > 1) {
        control.mBraceB = first;
        control.mIndexB = 1;
    } else {
        control.mBraceB = first->mPrev;
        control.mIndexB = 0;
    }
    s3DPoint cur;   // sp+0xc8
    s3DPoint next;  // sp+0xbc
    GetTrackPosition(&control, 0.0f, &cur);
    const float length = instance->mFAC;
    float total = 0.0f;
    if (control.mBraceB == nullptr) return;

    int32_t estimate = CalcNumTrackSamples(DataInstance()->mLastBrace, length, &total);
    if (static_cast<uint32_t>(estimate) > *CapacityWord(instance)) {
        MReleaseFreeF(instance->mSamples);
        const float scaled = 1.1f * static_cast<float>(static_cast<uint32_t>(estimate));
        *CapacityWord(instance) = static_cast<uint32_t>(FloatToCell(static_cast<double>(scaled)));
        instance->mSamples = static_cast<sTrackSample*>(MReleaseAllocF(*CapacityWord(instance) * static_cast<uint32_t>(sizeof(sTrackSample)), 1));
    }
    bool firstSample = true;
    bool wrapping = true;
    const float step = 0.05f;
    float alignStep = step;       // f23
    float t = step;               // f25
    sTrackSample* out = instance->mSamples;
    float prevT = 0.0f;           // f26
    float accumulated = 0.0f;     // f27
    float previousAccumulated = 0.0f;  // f29
    {
        tag_sBezierControl* a = ControlAt(control.mBrace->mF90->mControlsA, control.mIndex);
        tag_sBezierControl* b = ControlAt(control.mBrace->mF90->mControlsB, control.mIndex);
        a->mF08 = prevT;
        b->mF08 = prevT;
    }

    while (control.mBraceB != nullptr || wrapping) {
        if (control.mBraceB != nullptr) {
            GetTrackPosition(&control, t, &next);
        } else {
            GetWrappedTrackPosition(&control, instance->mLastBrace->mPrev, t, &next);
        }
        const float dy = cur.y - next.y;
        const float dx = cur.x - next.x;
        const float dz = cur.z - next.z;
        double sum = ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(dy * dy));
        sum = static_cast<double>(dz * dz) + sum;
        const float dist = static_cast<float>(std::sqrt(sum));
        accumulated = accumulated + dist;

        if (firstSample || accumulated >= length) {
            float ratio;
            if (firstSample) {
                firstSample = false;
                ratio = 0.0f;
            } else {
                ratio = (length - previousAccumulated) / dist;
            }
            const float inverse = 1.0f - ratio;
            prevT = ppc_fmadds(prevT, inverse, t * ratio);
            cur.x = ppc_fmadds(cur.x, inverse, next.x * ratio);
            cur.y = ppc_fmadds(cur.y, inverse, next.y * ratio);
            cur.z = ppc_fmadds(cur.z, inverse, next.z * ratio);
            const double rem = std::fmod(static_cast<double>(prevT), static_cast<double>(step));
            accumulated = 0.0f;
            alignStep = step - static_cast<float>(rem);
            out->mPos = cur;
            t = prevT + alignStep;
            alignStep = step;
            tag_sBezierControl* ctlA = ControlAt(control.mBrace->mF90->mControlsA, control.mIndex);
            tag_sBezierControl* ctlB = ControlAt(control.mBrace->mF90->mControlsB, control.mIndex);
            out->mPiece = reinterpret_cast<sTrackSamplePiece*>(ctlA);
            out->mPieceB = ctlB;
            SampleT(out) = prevT;
            while (SampleT(out) < 0.0f) {
                tag_sBezierControl* link = Link14(SampleA(out));
                if (link != nullptr) out->mPiece = reinterpret_cast<sTrackSamplePiece*>(link);
                link = Link14(SampleB(out));
                if (link != nullptr) out->mPieceB = link;
                SampleT(out) = SampleT(out) + 1.0f;
            }
            tag_sBezierControl* current = SampleA(out);
            const float place = current->mF08;
            if (FloatToCell(static_cast<double>(place)) != 0) {
                const uint32_t cell = static_cast<uint32_t>(FloatToCell(static_cast<double>(place)));
                const int32_t index = static_cast<int32_t>(out - instance->mSamples);
                if (cell == static_cast<uint32_t>(index)) {
                    const uint32_t again = static_cast<uint32_t>(FloatToCell(static_cast<double>(place)));
                    current->mF08 = static_cast<float>(again) - 0.001f;
                    SampleB(out)->mF08 = SampleA(out)->mF08;
                }
            }

            sTrackSample* sample = out;
            sOrientationBlock block;
            s3DPoint tangent;
            if (control.mBraceB != nullptr) {
                BezierTangent(SampleA(sample), SampleA(sample)->mNext, SampleT(sample), &tangent);
            } else {
                BezierTangent(SampleA(sample), instance->mLastBrace->mPrev->mF90->mControlsA, SampleT(sample), &tangent);
            }
            const uint32_t big = 0x7f7fffffu;
            block.mOrient.mF00 = 0.0f;
            block.mOrient.mF04 = block.mOrient.mF08 = block.mOrient.mF0C = 0.0f;
            block.mOrient.mF10 = block.mOrient.mF14 = block.mOrient.mF18 = 0.0f;
            std::memcpy(&block.mExtra, &big, sizeof big);
            GetAlignmentParamsQuick(tangent, SampleT(sample), &block.mOrient, nullptr);
            if (control.mBraceB != nullptr) {
                GetBankParams(&sample->mPos, SampleB(sample), SampleB(sample)->mNext, SampleT(sample), &block.mOrient);
            }
            sample->mYaw = block.mOrient.mF04;
            sample->mPitch = block.mOrient.mF08;
            sample->mBank = block.mOrient.mF0C;
            if (control.mBraceB == nullptr) wrapping = false;
            ++out;
        } else {
            prevT = t;
            t = t + alignStep;
            cur = next;
        }

        const float one = 1.0f;
        if (t > one) {
            if (control.mBraceB != nullptr) {
                IncrementTrackPosnControl(&control);
                prevT = prevT - one;
                t = t - one;
                tag_sBezierControl* ctlA = ControlAt(control.mBrace->mF90->mControlsA, control.mIndex);
                tag_sBezierControl* ctlB = ControlAt(control.mBrace->mF90->mControlsB, control.mIndex);
                float v[3];
                v[0] = ctlA->mF40.x - cur.x;
                v[1] = ctlA->mF40.y - cur.y;
                v[2] = ctlA->mF40.z - cur.z;
                if (control.mBraceB != nullptr) {
                    GetTrackPosition(&control, t, &next);
                    const float ey = next.y - cur.y;
                    const float ex = next.x - cur.x;
                    const float ez = next.z - cur.z;
                    float u[3] = {v[0], v[1], v[2]};
                    double s = ppc_fmadd(static_cast<double>(ex), static_cast<double>(ex), static_cast<double>(ey * ey));
                    s = static_cast<double>(ez * ez) + s;
                    const float elen = static_cast<float>(std::sqrt(s));
                    const float nx = ex / elen;
                    const float ny = ey / elen;
                    const float nz = ez / elen;
                    double s2 = ppc_fmadd(static_cast<double>(u[0]), static_cast<double>(u[0]), static_cast<double>(u[1] * u[1]));
                    s2 = static_cast<double>(u[2] * u[2]) + s2;
                    const float ulen = static_cast<float>(std::sqrt(s2));
                    u[0] = u[0] / ulen;
                    u[1] = u[1] / ulen;
                    u[2] = u[2] / ulen;
                    if (!std::isnan(u[0])) {
                        const float dot = ppc_fmadds(nz, u[2], ppc_fmadds(nx, u[0], ny * u[1]));
                        v[0] = v[0] * dot;
                        v[1] = v[1] * dot;
                        v[2] = v[2] * dot;
                    }
                }
                double s3 = ppc_fmadd(static_cast<double>(v[0]), static_cast<double>(v[0]), static_cast<double>(v[1] * v[1]));
                s3 = static_cast<double>(v[2] * v[2]) + s3;
                const float vlen = static_cast<float>(std::sqrt(s3));
                const float ratio = (vlen + accumulated) / instance->mFAC;
                const int32_t index = static_cast<int32_t>(out - instance->mSamples);
                if (index != 0) {
                    ctlA->mF08 = ratio + static_cast<float>(static_cast<uint32_t>(index - 1));
                } else {
                    ctlA->mF08 = ratio;
                }
                ctlB->mF08 = ctlA->mF08;
                if (control.mBraceB == nullptr) instance->mLength = ctlA->mF08;
                tag_sTrackBrace* brace = ctlA->mBrace;
                if (brace->mF74 != nullptr && (brace->mFlags & 0x2000u) == 0) {
                    if (*brace->mF74 == control.mIndex) brace->mSample = static_cast<uint32_t>(FloatToCell(static_cast<double>(ctlA->mF08)));
                } else {
                    brace->mSample = static_cast<uint32_t>(FloatToCell(static_cast<double>(ctlA->mF08)));
                }
            }
        } else if (t < 0.0f) {
            if (control.mBraceB != nullptr) {
                DecrementTrackPosnControl(&control);
                prevT = prevT + one;
                t = t + one;
            }
        }
        previousAccumulated = accumulated;
    }

    *SpanWord(instance) = 0;
    tag_sTrackBrace* previous = DataInstance()->mLastBrace;
    for (tag_sTrackBrace* brace = previous->mPrev; brace != nullptr; brace = brace->mPrev) {
        const uint32_t span = brace->mSample - previous->mSample + 1;
        if (span > *SpanWord(instance)) *SpanWord(instance) = span;
        previous = brace;
    }
    instance->mSampleCount = static_cast<uint32_t>(out - instance->mSamples);
    *out = out[-1];
    double whole;
    const float fraction = static_cast<float>(std::modf(static_cast<double>(instance->mLength), &whole));
    int32_t cells = ppc_fctiwz(whole);
    if (fraction != 0.0f) cells = cells + 1;
    if (!(cells < static_cast<int32_t>(instance->mSampleCount))) instance->mSampleCount = instance->mSampleCount + 1;
    SetEntranceLinearSamples(instance);
}

// GenerateTrackSubdivisions 0x1004e8c0, CoasterSetSpeed (instance overload) 0x1004b94c, GetBankAtBrace 0x1004b1f8,
// FlashEndPylons 0x10052158, DecCoasterConst1 0x100524a0.

// 0x10052158. Stops the flashing of the current end pylon's set, then flashes the one the mode names (1 the first, 2 the last).
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) {
    if (instance->mFlashBrace != nullptr) {
        tag_sRideAnimSet* set = gRideAnimSets[instance->mFlashBrace->mWord70];
        if ((set->mFlags & 0x4000000) != 0) StopFlashingObject(set);
        instance->mFlashBrace = nullptr;
    }
    tag_sTrackBrace* brace = nullptr;
    if ((mode & 1) != 0) {
        if ((instance->mBlock3C & 2) != 0) brace = instance->mPreviewBrace;
        else brace = instance->mBraces;
    } else if ((mode & 2) != 0) {
        if ((instance->mBlock3C & 2) != 0) brace = instance->mBrace64;
        else brace = instance->mLastBrace;
    }
    if (brace != nullptr) {
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        if ((set->mFlags & 0x4000000) == 0) StartFlashingObject(set);
        instance->mFlashBrace = brace;
    }
}

// 0x10052290
tag_sTrackBrace* GetCurSelectedTrackBrace() { return gCoasterDragBrace; }

// 0x100522cc
void SetCurSelectedTrackBrace(tag_sTrackBrace* brace) { gCoasterDragBrace = brace; }

// TPConfig.cpp (main app): the coaster editor's data change and the loft drag toggle. Unit v5054.
//   HandleCoasterDataChange 0x10052318, ToggleLoftPylonState 0x10052ac4

// 0x10052318: flags bit 0 the loft changed, bit 2 the wobble changed; b nonzero skips the collision flag.
void HandleCoasterDataChange(uint32_t flags, tag_sTrackBrace* brace, tag_sCoasterInstance* instance, int32_t a, int32_t b) {
    (void)a;
    const sTrackBraceData* pylon = brace->mData;
    sPylonControl* control = ControlOf(pylon);
    if ((flags & 1) != 0) {
        if (brace == instance->mPreviewBrace) {
            float f = brace->mFE8w;
            if (f < 0.0f) {
                f = 0.0f;
            } else if (f > 1.0f) {
                f = 1.0f;
            }
            if ((control->mFlags & 0x4000) == 0) instance->mF118 = GetHeightForInterval(f, control);
        }
        if ((instance->mFlags & 0x800) != 0) DoImmediatePhysicsFeedback(instance);
    } else if ((flags & 4) != 0 && (brace->mFlags & 0x40) == 0 && brace == instance->mPreviewBrace) {
        instance->mF11C = brace->mFD8;
    }
    FlagGenerateTrack(instance, 1);
    if ((flags & 1) != 0) {
        SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(instance->mType)), brace);
        UpdatePylonStackMeshes(control, const_cast<tag_sPylon*>(pylon), 0, 0);
    }
    if (b == 0) FlagCollisionUpdateTrackAroundBrace(brace);
}

// 0x100524a0. Key action: divides the edited coaster type's curviness by 1.05, traces both values, flags the track.
int32_t DecCoasterConst1() {
    if (gCoasterEditInstance != nullptr) {
        tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
        if (type != nullptr) {
            type->mF08 = type->mF08 / 1.05f;
            type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
            CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type->mF08), static_cast<double>(type->mF0C));
            FlagGenerateTrack(gCoasterEditInstance, 1);
        }
    }
    return 1;
}

// 0x10052564 (156 bytes): the coaster's type words +8 and +0xc are floats.
int32_t IncCoasterConst1() {
    if (gCoasterEditInstance != nullptr) {
        uint8_t* type = reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
        if (type != nullptr) {
            float* a = reinterpret_cast<float*>(type + 8);
            *a = *a * 1.05f;
            type = reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
            CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(*reinterpret_cast<float*>(type + 8)),
                   static_cast<double>(*reinterpret_cast<float*>(type + 0xc)));
            FlagGenerateTrack(gCoasterEditInstance, 1);
        }
    }
    return 1;
}

// Unit v5967: AbortMovePylon 0x10057df4, SingleHFieldTextureChange 0x1005b840, PlacePylon (4 arguments) 0x10052a30,
// IsCoasterUseable 0x10052954, IsTrackValid 0x100528d8, IsTrackClosed (void*) 0x10052850, DecCoasterConst2 0x10052628.

// 0x10052780 (36 bytes, stand-in): the instance overload of IsTrackClosed.
int32_t IsTrackClosed(tag_sCoasterInstance* instance);

namespace {
// The entrance id of a ride object sits at +0x1c.
uint32_t EntranceOf(const void* object) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(object) + 0x1c); }
}  // namespace

// 0x10052628
int32_t DecCoasterConst2() {
    if (gCoasterEditInstance != nullptr && gCoasterEditInstance->mType != nullptr) {
        const float* type = reinterpret_cast<const float*>(gCoasterEditInstance->mType);
        CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type[2]), static_cast<double>(type[3]));
        FlagGenerateTrack(gCoasterEditInstance, 1);
    }
    return 1;
}

// 0x100526d4
int32_t IncCoasterConst2() {
    if (gCoasterEditInstance != nullptr && gCoasterEditInstance->mType != nullptr) {
        const float* type = reinterpret_cast<const float*>(gCoasterEditInstance->mType);
        CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type[2]), static_cast<double>(type[3]));
        FlagGenerateTrack(gCoasterEditInstance, 1);
    }
    return 1;
}

// 0x10052780 (the overload taking the instance; the one taking the coaster's object is 0x10052850)
int32_t IsTrackClosed(tag_sCoasterInstance* instance) {
    uint32_t flags = instance->mBlock3C;
    if ((flags & 1) && !(flags & 2)) return 1;
    return 0;
}

// 0x10052850
int32_t IsTrackClosed(void* coaster) {
    if (coaster == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(coaster));
    if (instance == nullptr) return 0;
    uint32_t flags = instance->mBlock3C;
    if ((flags & 1) && !(flags & 2)) return 1;
    return 0;
}

// 0x100527e0
tag_sCoasterInstance* GetCoasterInstanceFromCObject(void* object) {
    if (object == nullptr) return nullptr;
    uint32_t entrance;
    std::memcpy(&entrance, static_cast<uint8_t*>(object) + 0x1c, 4);
    return FindCoasterInstanceFromEntranceID(entrance);
}

// 0x100528d8
int32_t IsTrackValid(void* coaster) {
    if (coaster == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(coaster));
    if (instance == nullptr) return 0;
    return *reinterpret_cast<const uint32_t*>(&instance->mF140[0]) == 0 ? 1 : 0;
}

// 0x10052954
int32_t IsCoasterUseable(CObject* object) {
    if (object == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(object));
    if (instance == nullptr) return 0;
    bool answer = false;
    bool closed = false;
    if (*reinterpret_cast<const uint32_t*>(&instance->mF140[0]) == 0 && IsTrackClosed(instance) != 0) closed = true;
    if (closed && instance != gCoasterEditInstance) answer = true;
    return answer;
}
