// Test library for CalcNumTrackSamples (unit v5524; libtpw_unit_CalcNumTrackSamples.dylib), main app:
//   src/DestroyCoasterPylons_CalcNumTrackSamples.cpp
//
// The callees (GetTrackPosition, DistanceBetweenPoints, FloatToCell, IncrementTrackPosnControl) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

namespace {
uint32_t Num(const tag_sTrackBrace* b) { return b ? b->mNumber : 0; }
}  // namespace

void GetTrackPosition(sTrackPosnControl* c, float t, s3DPoint* point) {
    ev("GetTrackPosition", {c->mIndex, Num(c->mBrace), c->mIndexB, Num(c->mBraceB), fbits(t)});
    uint32_t b[3] = {static_cast<uint32_t>(ret("GTP")), static_cast<uint32_t>(ret("GTP")), static_cast<uint32_t>(ret("GTP"))};
    std::memcpy(point, b, 12);
}

float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    uint32_t w[6];
    std::memcpy(w, a, 12);
    std::memcpy(w + 3, b, 12);
    ev("Distance", {w[0], w[1], w[2], w[3], w[4], w[5]});
    uint32_t r = static_cast<uint32_t>(ret("Dist"));
    float f;
    std::memcpy(&f, &r, 4);
    return f;
}

int32_t FloatToCell(double v) {
    uint64_t q;
    std::memcpy(&q, &v, 8);
    ev("FloatToCell", {static_cast<int64_t>(q >> 32), static_cast<int64_t>(q & 0xffffffffu)});
    return static_cast<int32_t>(ret("Cell"));
}

void IncrementTrackPosnControl(sTrackPosnControl* c) {
    ev("Increment", {c->mIndex, Num(c->mBrace), c->mIndexB, Num(c->mBraceB)});
    c->mIndex = c->mIndex + 1;
    if (ret("Inc") == 0) c->mBraceB = nullptr;
}

extern "C" {

uint64_t tpw_ccn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ccn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    return n;
}

uint64_t tpw_ccn_brace(uint32_t number, uint64_t prev, uint32_t count) {
    tag_sTrackBrace* b = static_cast<tag_sTrackBrace*>(tpw::ArenaAllocZeroed(sizeof(tag_sTrackBrace)));
    sBraceSpline* s = static_cast<sBraceSpline*>(tpw::ArenaAllocZeroed(sizeof(sBraceSpline)));
    s->mCount = count;
    b->mNumber = number;
    b->mPrev = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(prev));
    b->mF90 = s;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(b));
}

int32_t tpw_ccn_run(uint64_t brace, float step, uint64_t total) {
    return CalcNumTrackSamples(reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)), step,
                               reinterpret_cast<float*>(static_cast<uintptr_t>(total)));
}

}  // extern "C"
