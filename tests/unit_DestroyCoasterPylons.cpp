// Test library for the coaster destroyers and the track position control (unit v5038; libtpw_unit_DestroyCoasterPylons.dylib,
// tests/test_DestroyCoasterPylons.py), main app:
//   src/game/rides/CoasterTrack.cpp   DestroyCoasterPylons, DestroyCoasterTrack, Increment/DecrementTrackPosnControl, GetTrackPosition
//
// The callees outside the unit are fakes that record what the real ones read. Objects are named in the log by the id word the test puts at
// their word 0 (a pylon control: its word +4; a node: its +8), never by address.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

namespace {

uint32_t Word(const void* p, size_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const char*>(p) + offset, 4);
    return v;
}

}  // namespace

// ---- the fakes
void DestroyTrackBrace(tag_sTrackBrace* brace, sPylonControl* pylon) {
    ev("DestroyTrackBrace", {static_cast<int64_t>(Word(brace, 0)), static_cast<int64_t>(Word(pylon, 4)), static_cast<int64_t>(Word(pylon, 0))});
}

void Destroy3DObjectFromAIWorld(uint32_t a, uint32_t b) {
    ev("Destroy3DObjectFromAIWorld", {static_cast<int64_t>(a), static_cast<int64_t>(b)});
}

void DestroyBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info) {
    ev("DestroyBraceInfo", {static_cast<int64_t>(Word(brace, 0)), static_cast<int64_t>(reinterpret_cast<const char*>(info) - reinterpret_cast<const char*>(brace))});
}

void DestroyDynamicMeshSetInstance(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags) {
    ev("DestroyDynamicMeshSetInstance", {static_cast<int64_t>(Word(set, 0)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(heap)), static_cast<int64_t>(flags)});
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {static_cast<int64_t>(Word(block, 8))});
    return nullptr;
}

// Writes the controls' ids (their word +8) and t into the point, so the compare sees which controls were passed.
void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point) {
    uint32_t words[3] = {Word(a, 8), Word(b, 8), 0};
    std::memcpy(&words[2], &t, 4);
    std::memcpy(point, words, 12);
    ev("BezierPosition", {static_cast<int64_t>(words[0]), static_cast<int64_t>(words[1]), static_cast<int64_t>(words[2])});
}

extern "C" {

uint64_t tpw_dcp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module writes by (the Mac's are constants in the module).
int32_t tpw_dcp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF38));     // 0
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPylons));  // 1
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mFlags));          // 2
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mF08));            // 3
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mF0C));            // 4
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mF10));            // 5
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mControl));        // 6
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mNext));           // 7
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mCount));          // 8
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mBraces));         // 9
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mF38));            // 10
    out[n++] = static_cast<int64_t>(offsetof(sTrackPosnControl, mBrace));      // 11
    out[n++] = static_cast<int64_t>(offsetof(sTrackPosnControl, mIndexB));     // 12
    out[n++] = static_cast<int64_t>(offsetof(sTrackPosnControl, mBraceB));     // 13
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));         // 14
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));         // 15
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF90));          // 16
    out[n++] = static_cast<int64_t>(offsetof(sBraceSpline, mControlsA));       // 17
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));             // 18
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));                  // 19
    out[n++] = static_cast<int64_t>(sizeof(tag_sDMeshSet));                    // 20
    return n;
}

void tpw_dcp_pylons(uint64_t instance) { DestroyCoasterPylons(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
void tpw_dcp_track(uint64_t instance) { DestroyCoasterTrack(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
void tpw_dcp_inc(uint64_t control) { IncrementTrackPosnControl(reinterpret_cast<sTrackPosnControl*>(control)); }
void tpw_dcp_dec(uint64_t control) { DecrementTrackPosnControl(reinterpret_cast<sTrackPosnControl*>(control)); }
void tpw_dcp_pos(uint64_t control, float t, uint64_t point) {
    GetTrackPosition(reinterpret_cast<sTrackPosnControl*>(control), t, reinterpret_cast<s3DPoint*>(point));
}

}  // extern "C"
