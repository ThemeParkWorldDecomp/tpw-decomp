#include "TPCoaster.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>

#include "platform/arena.h"
#include "ppc_math.h"

float gExtrudeBlendMin = 0.0f;         // 0x1029ce70
float gExtrudeBlendMax = 1.0f;         // 0x1029ce74
sExtrusionParams gExtrudeParamsBuf[2560];   // 0x102b7674

namespace {

// The words the type's table keeps are Mac addresses: on the host they are arena offsets (0 is null); the out records keep native pointers.
sCoasterRecord* AsRecord(uint32_t word) {
    return word != 0 ? reinterpret_cast<sCoasterRecord*>(tpw::gArenaBase + word) : nullptr;
}

float RecordLength(const sCoasterRecord* record) {   // the record's word +0x14 (mF14), a float
    float value;
    std::memcpy(&value, &record->mF14, sizeof(value));
    return value;
}

// The key of a track position: the sample number (the whole part, indexing the samples) plus how far the position is between the sample's
// control and the next one's; the control list is walked while the next control's place is below the position.
float KeyOf(float position) {
    sTrackSample* samples = gCoasterCurrent->mSamples;
    // The call stays a call (the optimiser merges calls of modf with the same argument, and the original makes every one).
    static double (*volatile callModf)(double, double*) = static_cast<double (*)(double, double*)>(&std::modf);
    double whole;
    callModf(static_cast<double>(position), &whole);
    const int32_t index = ppc_fctiwz(whole);
    tag_sBezierControl* node = reinterpret_cast<tag_sBezierControl*>(samples[index].mPiece);
    tag_sBezierControl* next;
    for (;;) {
        next = node->mNext;
        if (next == nullptr) break;
        if (!(next->mF08 < position)) break;
        node = next;
    }
    float ratio;
    if (next != nullptr) {
        const float from = node->mF08;
        const float a = position - from;
        const float b = next->mF08 - from;
        ratio = a / b;
    } else {
        ratio = 0.0f;
    }
    uint32_t number;
    std::memcpy(&number, node, sizeof(number));
    return ratio + static_cast<float>(number);
}

}  // namespace

// 0x1004ff18 (2,536 bytes). Mac registers: r23 out, r22 branch, r19 inBranch, r25 previous answer, r26 new answer, r24 the word the out record keeps.
void GenerateExtrusions() {
    SetInitialCoasterInterpParams(static_cast<sCoasterTrackParams*>(gCoasterCurrentData));
    sCoasterType* type = reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterCurrent->mType));
    sCoasterOrientation* orient = reinterpret_cast<sCoasterOrientation*>(
        static_cast<char*>(gCoasterCurrentData) + (offsetof(tag_sCoasterInstance, mF78) - offsetof(tag_sCoasterInstance, mBlock3C)));
    float pos = 0.0f;
    bool inBranch = false;
    sExtrusionParams* branch = nullptr;
    sExtrusionParams* out = gExtrudeParamsBuf;
    out->mF00 = 0.0f;
    out->mF04 = 0.0f;
    uint32_t previous = static_cast<uint32_t>(type->mTable[0].mAnswer);
    out->mSection = AsRecord(previous);
    sExtrusionParams& second = gExtrudeParamsStatic[1];
    second.mF04 = 10000000.0f;
    second.mF00 = 10000000.0f;
    second.mSection = *reinterpret_cast<sCoasterRecord**>(reinterpret_cast<char*>(type) + kCoasterTypeSectionB);
    second.mF08 = 0.0f;
    sExtrusionParams& first = gExtrudeParamsStatic[0];
    first.mF04 = 10000000.0f;
    first.mF00 = 10000000.0f;
    first.mF08 = 0.0f;
    first.mSection = *reinterpret_cast<sCoasterRecord**>(reinterpret_cast<char*>(type) + kCoasterTypeSectionA);

    while (!IsEndOfSampleBuffer(gCoasterCurrent, pos)) {
        s3DPoint point;
        InterpolateCoasterSpline(gCoasterCurrent, pos, &point, orient);
        // The call stays a call of fmod (the double one): the optimiser would narrow it to fmodf, as both arguments are floats.
        static double (*volatile callFmod)(double, double) = static_cast<double (*)(double, double)>(&std::fmod);
        orient->mF0C = static_cast<float>(callFmod(static_cast<double>(orient->mF0C), 3.1415927410125732));
        const uint32_t answer = static_cast<uint32_t>(GetCrossSectionFromParams(type, pos, orient, 0.0f));
        uint32_t keep = answer;
        if (answer != previous) {
            uint32_t flags;
            std::memcpy(&flags, type->mF00 + 0x74, sizeof(flags));
            if ((flags & 1) == 0) {
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                ++out;
            } else {
                sCoasterRecord* transition = nullptr;
                uint32_t transitionWord = 0;
                for (uint32_t i = 0; i < type->mCount; ++i) {
                    const sCoasterCrossSectionEntry* entry = &type->mTable[i];
                    uint32_t from, to;
                    std::memcpy(&from, entry->mF24, sizeof(from));
                    std::memcpy(&to, entry->mF24 + 4, sizeof(to));
                    if (from == previous && to == answer) {
                        transitionWord = static_cast<uint32_t>(entry->mAnswer);
                        transition = AsRecord(transitionWord);
                        break;
                    }
                }
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                ++out;
                if (transition != nullptr) {
                    const float length = RecordLength(transition);
                    if (length > 0.0f) {
                        keep = transitionWord;
                        if (inBranch) {
                            branch->mF04 = pos;
                            branch->mF00 = KeyOf(branch->mF04);
                        }
                        inBranch = true;
                        out->mF04 = pos + RecordLength(transition);
                        out->mF00 = KeyOf(out->mF04);
                        out->mF08 = 1.0f;
                        branch = out;
                    } else {
                        out->mSection = transition;
                        const float end = pos + length;
                        if (branch != nullptr && branch->mF04 > end) {
                            if (inBranch) {
                                const float lengthB = std::fabs(RecordLength(branch->mSection));
                                const float lengthA = std::fabs(RecordLength(transition));
                                const float lowest = branch->mF04;
                                const float f1 = lowest - lengthB;
                                const float denominator = lengthB + lengthA;
                                const float f1b = pos - f1;
                                float ratio = f1b / denominator;
                                ratio = (ratio < gExtrudeBlendMin) ? gExtrudeBlendMin : ratio;
                                ratio = (gExtrudeBlendMax < ratio) ? gExtrudeBlendMax : ratio;
                                branch->mF08 = ratio;
                                out->mF08 = ratio;
                                branch->mF04 = ppc_fnmsubs(1.0f - ratio, lengthB, branch->mF04);
                                branch->mF00 = KeyOf(branch->mF04);
                                out->mF04 = pos;
                                out->mF00 = KeyOf(out->mF04);
                                ++out;
                            } else {
                                out->mF04 = pos;
                                out->mF00 = KeyOf(out->mF04);
                                out->mF08 = 1.0f;
                                ++out;
                            }
                        } else {
                            out[-1].mF04 = pos + RecordLength(transition);
                            out[-1].mF00 = KeyOf(out[-1].mF04);
                            out->mF04 = pos;
                            out->mF00 = KeyOf(out->mF04);
                            out->mF08 = 1.0f;
                            ++out;
                        }
                        inBranch = false;
                    }
                    branch = out;
                }
            }
            previous = answer;
            out->mSection = AsRecord(keep);
        }
        if (inBranch) {
            const float limit = branch->mF04;
            if (pos > limit || pos == limit) {
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                inBranch = false;
                out[1].mSection = AsRecord(answer);
                ++out;
            }
        }
        pos = pos + 0.25f;
    }
    out->mF04 = (1.0f + pos) + 0.25f;
    out->mF00 = 100000.0f;
}
