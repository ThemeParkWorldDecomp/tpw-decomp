// Test library for LinearSampleTrack (unit v5664; libtpw_unit_LinearSampleTrack.dylib, tests/test_LinearSampleTrack.py), main app:
//   src/game/rides/CoasterTrack.cpp   LinearSampleTrack
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; gCoasterCurrentData is
// defined here. The position control fakes only move the control's numbers (the test's queue "inc" says how).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

void* gCoasterCurrentData;

namespace {

uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &b, 4);
    return value;
}

// A smooth track: the point is the direction (3, 1, -2) times the case's scale times (t + the control's index).
void PutPoint(s3DPoint* point, float t, uint32_t index) {
    const float scale = FromBits(ret("scale"));
    const float tt = t + static_cast<float>(index);
    const float ax = scale * 3.0f;
    const float ay = scale * 1.0f;
    const float az = scale * -2.0f;
    point->x = tt * ax;
    point->y = tt * ay;
    point->z = tt * az;
}

int64_t I(int64_t v) { return v; }

void WordsEvent(const char* name, const void* p, int n) {
    uint32_t w[8];
    std::memcpy(w, p, 4 * n);
    if (n == 8) ev(name, {I(w[0]), I(w[1]), I(w[2]), I(w[3]), I(w[4]), I(w[5]), I(w[6]), I(w[7])});
    else ev(name, {I(w[0]), I(w[1]), I(w[2])});
}

}  // namespace

void GetTrackPosition(sTrackPosnControl* c, float t, s3DPoint* point) {
    ev("GTP", {I(c->mIndex), I(lab(c->mBrace)), I(c->mIndexB), I(lab(c->mBraceB)), I(Bits(t))});
    PutPoint(point, t, c->mIndex);
}

void GetWrappedTrackPosition(sTrackPosnControl* c, tag_sTrackBrace* brace, float t, s3DPoint* point) {
    ev("GWTP", {I(c->mIndex), I(lab(c->mBrace)), I(c->mIndexB), I(lab(c->mBraceB)), I(lab(brace)), I(Bits(t))});
    PutPoint(point, t, c->mIndex);
}

int32_t CalcNumTrackSamples(tag_sTrackBrace* brace, float step, float* total) {
    ev("CNTS", {lab(brace), Bits(step), Bits(*total)});
    return static_cast<int32_t>(ret("cnts"));
}

void IncrementTrackPosnControl(sTrackPosnControl* c) {
    ev("INC", {I(c->mIndex), I(lab(c->mBrace)), I(c->mIndexB), I(lab(c->mBraceB))});
    int64_t v = ret("inc");
    c->mIndex += 1;
    if (v & 1) c->mBraceB = nullptr;
    if ((v & 2) && c->mBrace->mPrev != nullptr) c->mBrace = c->mBrace->mPrev;
}

void DecrementTrackPosnControl(sTrackPosnControl* c) {
    ev("DEC", {I(c->mIndex), I(lab(c->mBrace)), I(c->mIndexB), I(lab(c->mBraceB))});
    if (c->mIndex > 0) c->mIndex -= 1;
}

void BezierTangent(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* tangent) {
    ev("BT", {lab(a), lab(b), Bits(t)});
    tangent->x = FromBits(ret("tan"));
    tangent->y = FromBits(ret("tan"));
    tangent->z = FromBits(ret("tan"));
}

void GetAlignmentParamsQuick(s3DPoint tangent, float twist, sCoasterOrientation* orient, float* out) {
    ev("GAQ", {I(Bits(tangent.x)), I(Bits(tangent.y)), I(Bits(tangent.z)), I(Bits(twist)), I(out != nullptr ? 1 : 0)});
    WordsEvent("GAQo", orient, 8);
    orient->mF04 = FromBits(ret("al"));
    orient->mF08 = FromBits(ret("al"));
    orient->mF0C = FromBits(ret("al"));
}

void GetBankParams(s3DPoint* point, tag_sBezierControl* c, tag_sBezierControl* d, float t, sCoasterOrientation* orient) {
    ev("GBP", {I(Bits(point->x)), I(Bits(point->y)), I(Bits(point->z)), I(lab(c)), I(lab(d)), I(Bits(t))});
    WordsEvent("GBPo", orient, 8);
    orient->mF0C = FromBits(ret("bank"));
}

void* MReleaseFreeF(void* block) {
    ev("FREE", {lab(block)});
    return nullptr;
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("ALLOC", {static_cast<int64_t>(size / sizeof(sTrackSample)), flags, static_cast<int64_t>(size % sizeof(sTrackSample))});
    return tpw::ArenaAllocZeroed(size);
}

void SetEntranceLinearSamples(tag_sCoasterInstance* instance) { ev("SEL", {lab(instance)}); }

int32_t FloatToCell(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("FTC", {static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xffffffffu)});
    if (!(value > -2147483649.0 && value < 2147483648.0)) return INT32_MIN;
    return static_cast<int32_t>(value);
}

extern "C" {

uint64_t tpw_lst_addr(const char* name) {
    if (std::strcmp(name, "data") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gCoasterCurrentData));
    return 0;
}

uint64_t tpw_lst_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets and sizes in the order of the module's MAC table.
int32_t tpw_lst_layout(int64_t* out) {
    int32_t n = 0;
    auto put = [&](size_t v) { out[n++] = static_cast<int64_t>(v); };
    put(offsetof(tag_sCoasterInstance, mBlock3C));
    put(offsetof(tag_sCoasterInstance, mLastBrace));
    put(offsetof(tag_sCoasterInstance, mF94) + 4);
    put(offsetof(tag_sCoasterInstance, mSamples));
    put(offsetof(tag_sCoasterInstance, mFA0));
    put(offsetof(tag_sCoasterInstance, mSampleCount));
    put(offsetof(tag_sCoasterInstance, mLength));
    put(offsetof(tag_sCoasterInstance, mFAC));
    put(sizeof(tag_sCoasterInstance));
    put(offsetof(tag_sTrackBrace, mFlags));
    put(offsetof(tag_sTrackBrace, mPrev));
    put(offsetof(tag_sTrackBrace, mF90));
    put(offsetof(tag_sTrackBrace, mF74));
    put(offsetof(tag_sTrackBrace, mSample));
    put(sizeof(tag_sTrackBrace));
    put(offsetof(sBraceSpline, mCount));
    put(offsetof(sBraceSpline, mControlsA));
    put(offsetof(sBraceSpline, mControlsB));
    put(sizeof(sBraceSpline));
    put(offsetof(tag_sBezierControl, mF08));
    put(offsetof(tag_sBezierControl, mBrace));
    put(offsetof(tag_sBezierControl, mF4C) + 4);
    put(offsetof(tag_sBezierControl, mNext));
    put(sizeof(tag_sBezierControl));
    put(offsetof(tag_sBezierControl, mF40));
    put(offsetof(sTrackSample, mPos));
    put(offsetof(sTrackSample, mYaw));
    put(offsetof(sTrackSample, mPitch));
    put(offsetof(sTrackSample, mBank));
    put(offsetof(sTrackSample, mPiece));
    put(offsetof(sTrackSample, mPieceB));
    put(offsetof(sTrackSample, mF34));
    put(sizeof(sTrackSample));
    return n;
}

void tpw_lst_run(uint64_t instance) { LinearSampleTrack(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }

}  // extern "C"
