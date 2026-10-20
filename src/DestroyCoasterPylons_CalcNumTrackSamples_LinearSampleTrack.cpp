#include <cstdint>
#include <cmath>
#include <cstddef>
#include <cstring>
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

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
