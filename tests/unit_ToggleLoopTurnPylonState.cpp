// Test library for the editor's pylon drag toggles (unit v5144; libtpw_unit_ToggleLoopTurnPylonState.dylib, tests/test_ToggleLoopTurnPylonState.py), main app:
//   src/HandleCoasterDataChange_ToggleLoopTurnPylonState.cpp   ToggleLoopTurnPylonState, ToggleRotatePylonState, ToggleBankInclinePylonState
//
// The callees outside the unit (CActionRecorder::RecordAction in its two float overloads, LockPointedAtRide) are fakes that record their
// arguments; the globals of other files the unit reads or writes are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCamera.h"
#include "TPCoaster.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
float gPointerScreenXY[2];
tag_sCoasterInstance* gCoasterEditInstance;
float gCoasterDragLastDX;
float gCoasterDragLastDY;
float gCoasterDragAnchor[2];
float gCoasterDragScaleX;
float gCoasterDragScaleY;
float gCoasterHorizontalBase2;
float gCoasterHorizontalBase1;
float gCoasterHorizontalMax1;
float gCoasterHorizontalMin1;
float gCoasterVerticalBase1;
float gCoasterVerticalMax1;
float gCoasterVerticalMin1;
float* gCoasterHorizontalTarget2;
float* gCoasterHorizontalTarget1;
float* gCoasterVerticalTarget2;
float* gCoasterVerticalTarget1;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterInputFlags;
CActionRecorder gActionRecorder;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void CActionRecorder::RecordAction(int32_t action, float value, uint16_t id) {
    ev("RecordAction3", {lab(this), action, CanonBits(value), id});
}

void CActionRecorder::RecordAction(RecordedAction action, float a, float b, uint16_t id) {
    ev("RecordAction4", {lab(this), static_cast<int32_t>(action), CanonBits(a), CanonBits(b), id});
}

void LockPointedAtRide(int32_t flag) { ev("LockPointedAtRide", {flag}); }

extern "C" {

uint64_t tpw_tlp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterEditInstance", &gCoasterEditInstance},
        {"gCoasterInputFlags", &gCoasterInputFlags},
        {"gCoasterDragBrace", &gCoasterDragBrace},
        {"gPointerScreenXY", gPointerScreenXY},
        {"gCoasterDragLastDX", &gCoasterDragLastDX},
        {"gCoasterDragLastDY", &gCoasterDragLastDY},
        {"gCoasterDragAnchor", gCoasterDragAnchor},
        {"gCoasterDragScaleX", &gCoasterDragScaleX},
        {"gCoasterDragScaleY", &gCoasterDragScaleY},
        {"gCoasterHorizontalBase2", &gCoasterHorizontalBase2},
        {"gCoasterHorizontalBase1", &gCoasterHorizontalBase1},
        {"gCoasterHorizontalMax1", &gCoasterHorizontalMax1},
        {"gCoasterHorizontalMin1", &gCoasterHorizontalMin1},
        {"gCoasterVerticalBase1", &gCoasterVerticalBase1},
        {"gCoasterVerticalMax1", &gCoasterVerticalMax1},
        {"gCoasterVerticalMin1", &gCoasterVerticalMin1},
        {"gCoasterHorizontalTarget2", &gCoasterHorizontalTarget2},
        {"gCoasterHorizontalTarget1", &gCoasterHorizontalTarget1},
        {"gCoasterVerticalTarget2", &gCoasterVerticalTarget2},
        {"gCoasterVerticalTarget1", &gCoasterVerticalTarget1},
        {"gActionRecorder", &gActionRecorder},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_tlp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the Mac-offset fields the module writes, in the module's OFF_KEYS order.
int32_t tpw_tlp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF40));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNumber));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFD4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mAngle));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFD8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFDC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFF8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mControl));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mAngles) + 8 * sizeof(float));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(sizeof(sPylonControl));
    return n;
}

uint32_t tpw_tlp_loop() { ToggleLoopTurnPylonState(); return 1; }
uint32_t tpw_tlp_rotate() { ToggleRotatePylonState(); return 1; }
uint32_t tpw_tlp_bank() { ToggleBankInclinePylonState(); return 1; }

}  // extern "C"
