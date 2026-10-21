// Test library for the track editor's brace placement (unit v5562; libtpw_unit_GetActualBraceTrackPositions.dylib,
// tests/test_GetActualBraceTrackPositions.py), main app:
//   src/game/rides/CoasterPylon.cpp   GetActualBraceTrackPositions, GetActualBraceTangents, GetTangentLengths, gBraceTrackScale2
//
// The callees outside the unit are fakes that record what the real ones read: YRotateLMatrixNoTrans, the two vector rotation functions
// (they answer from the test's queues) and GetTangentsFromZVector. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_matrix.h"

using namespace calllog;

// gTPGlobals' constructor lives in a file this library does not compile: the storage takes the symbol's name (zero at start like bss).
alignas(16) uint8_t sTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
tag_sCoasterInstance* gCoasterCurrent;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t WordAt(const void* p, size_t off) {
    uint32_t w;
    std::memcpy(&w, static_cast<const uint8_t*>(p) + off, 4);
    return w;
}

int64_t CanonWordAt(const void* p, size_t off) {
    uint32_t w;
    std::memcpy(&w, static_cast<const uint8_t*>(p) + off, 4);
    float f;
    std::memcpy(&f, &w, 4);
    return CanonBits(f);
}

float FloatFromBits(int64_t bits) {
    uint32_t w = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

}  // namespace

void YRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) {
    ev("YRotate", {CanonWordAt(matrix, 0x30), CanonWordAt(matrix, 0x34), CanonWordAt(matrix, 0x38), CanonBits(c), CanonBits(s)});
}

float GetVectorRotationAroundXAxisFromZ(s3DPoint& point) {
    ev("AxisX", {CanonBits(point.x), CanonBits(point.y), CanonBits(point.z)});
    return FloatFromBits(ret("AxisX"));
}

float GetVectorRotationAroundZAxisFromY(s3DPoint& point) {
    ev("AxisZ", {CanonBits(point.x), CanonBits(point.y), CanonBits(point.z)});
    return FloatFromBits(ret("AxisZ"));
}

void GetTangentsFromZVector(tag_sCoasterType* coaster, s3DPoint* point, tag_sBezierControl* control, float lengthA, float lengthB,
                            sPylonControl* pylon, s3DPoint* vecA, s3DPoint* vecB, int32_t flag, int32_t last) {
    ev("GTZV", {WordAt(coaster, 0), WordAt(pylon, 0), WordAt(control, 4), CanonBits(control->mF40.x), CanonBits(control->mF40.y),
                CanonBits(control->mF40.z), CanonBits(lengthA), CanonBits(lengthB), static_cast<int64_t>(static_cast<uint32_t>(flag)),
                static_cast<int64_t>(static_cast<uint32_t>(last))});
    (void)vecA;   // unwritten stack at the track's unlinked ends, and the real callee never reads them
    (void)vecB;
    ev("GTZVvec", {CanonBits(point->x), CanonBits(point->y), CanonBits(point->z)});
}

extern "C" {

uint64_t tpw_bt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTPGlobals", sTPGlobalsStorage},
        {"gCoasterCurrent", &gCoasterCurrent},
        {"scale2", &gBraceTrackScale2},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets and sizes in the order of the module's KEYS.
int32_t tpw_bt_layout(int64_t* out) {
    int32_t n = 0;
    auto put = [&](size_t v) { out[n++] = static_cast<int64_t>(v); };
    put(offsetof(tag_sTrackBrace, mFlags));
    put(offsetof(tag_sTrackBrace, mPrev));
    put(offsetof(tag_sTrackBrace, mNext));
    put(offsetof(tag_sTrackBrace, mF90));
    put(offsetof(tag_sTrackBrace, mData));
    put(offsetof(tag_sTrackBrace, mAngle));
    put(offsetof(tag_sTrackBrace, mFE4));
    put(sizeof(tag_sTrackBrace) + 0x20);
    put(offsetof(sTrackBraceData, mFlags));
    put(offsetof(sTrackBraceData, mF18Table));
    put(sizeof(sTrackBraceData) + 0x20);
    put(offsetof(sBraceSpline, mControlsA));
    put(offsetof(sBraceSpline, mControlsB));
    put(sizeof(sBraceSpline));
    put(sizeof(tag_sBezierControl));
    put(offsetof(tag_sBezierControl, mF40));
    put(offsetof(tag_sRideAnimSet, mHeads));
    put(sizeof(tag_sRideAnimSet) + 0x20);
    put(offsetof(tag_sMesh, mF28));
    put(sizeof(tag_sMesh) + 0x20);
    put(offsetof(tagBumpHeadTable, entries));
    put(sizeof(tagBumpHeadTable));
    put(sizeof(tagBumpHeadEntry));
    put(offsetof(tagBumpHeadEntry, node));
    put(offsetof(tag_sCoasterInstance, mType));
    put(sizeof(tag_sCoasterInstance));
    put(offsetof(TPGlobals, mSinTableMask));
    put(offsetof(TPGlobals, mSinTableScale));
    put(offsetof(TPGlobals, mSinTable));
    return n;
}

void tpw_bt_tp(uint64_t set, uint64_t brace) {
    GetActualBraceTrackPositions(reinterpret_cast<tag_sRideAnimSet*>(set), reinterpret_cast<tag_sTrackBrace*>(brace));
}

void tpw_bt_tan(uint64_t brace, uint64_t set) {
    GetActualBraceTangents(reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<tag_sRideAnimSet*>(set));
}

void tpw_bt_len(uint32_t index, uint64_t brace, uint64_t before, uint64_t after, int32_t useA, uint64_t vecA, uint64_t vecB) {
    GetTangentLengths(index, reinterpret_cast<tag_sTrackBrace*>(brace), reinterpret_cast<float*>(before),
                      reinterpret_cast<float*>(after), useA, reinterpret_cast<s3DPoint*>(vecA), reinterpret_cast<s3DPoint*>(vecB));
}

}  // extern "C"
