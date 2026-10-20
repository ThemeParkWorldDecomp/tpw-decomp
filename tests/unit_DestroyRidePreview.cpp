// Test library for the ride preview's destroy, tick and the ride highlight (unit v5977; libtpw_unit_DestroyRidePreview.dylib,
// tests/test_DestroyRidePreview.py), main app:
//   src/Debug_overlays_DestroyRidePreview.cpp   DestroyRidePreview, HighlightRide, TickRidePreview
//
// The callees outside the unit (DestroyRideInstance, UnhighlightRide, CClock::Read, YRotateLMatrix, XRotateLMatrix) are fakes that record
// their arguments; the rotations also write two words of the matrix so that the code after them has something to work on. The globals of
// other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "Clock.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "LevelSystems.h"
#include "MainWindowCallback.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sRideAnimSet* gRideAnimSets[16];
tag_sRideAnimSet* gRideTypes[16];
tag_sRideAnimSet* gHighlightedRide;
tag_sRideAnimSet* gPotentialHighlightedRide;
int32_t gRideHighlightDisabled;
void* gHighlightNodeColours;
void* gHighlightFaceColours;
CClock gClock;
// gTPGlobals as raw zeroed storage under the program's symbol (the real constructor needs string classes this library does not link)
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {static_cast<int64_t>(instance)}); }

void UnhighlightRide() { ev("UnhighlightRide", {}); }

CFixedTime::CFixedTime() : mLastClock(0), mMilliseconds(0) {}   // gClock's member; never read here

CVariableTime::CVariableTime() : mLastClock(0), mMilliseconds(0), mRate(1.0) {}   // same

int32_t CClock::Read() {
    ev("ClockRead", {});
    return static_cast<int32_t>(ret("ClockRead"));
}

void YRotateLMatrix(sMatrix* matrix, float c, float s) {
    ev("YRotateLMatrix", {CanonBits(c), CanonBits(s)});
    matrix->m[0][0] = c;
    matrix->m[1][2] = s;
}

void XRotateLMatrix(sMatrix* matrix, float c, float s) {
    ev("XRotateLMatrix", {CanonBits(c), CanonBits(s)});
    matrix->m[2][2] = c;
    matrix->m[2][0] = s;
}

extern "C" {

uint64_t tpw_dr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rideSets", gRideAnimSets},
        {"rideTypes", gRideTypes},
        {"hl", &gHighlightedRide},
        {"pot", &gPotentialHighlightedRide},
        {"dis", &gRideHighlightDisabled},
        {"nodeCol", &gHighlightNodeColours},
        {"faceCol", &gHighlightFaceColours},
        {"tpg", gTPGlobalsStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_dr_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_dr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFD0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(sRidePreviewInfo));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableMask));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableScale));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTable));
    return n;
}

void tpw_dr_destroy(uint64_t info) { DestroyRidePreview(reinterpret_cast<sRidePreviewInfo*>(static_cast<uintptr_t>(info))); }

void tpw_dr_highlight() { HighlightRide(); }

void tpw_dr_tick(uint64_t info) { TickRidePreview(reinterpret_cast<sRidePreviewInfo*>(static_cast<uintptr_t>(info))); }

}  // extern "C"
