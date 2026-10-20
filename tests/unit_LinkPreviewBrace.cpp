// Test library for the track editor's preview brace (unit v5366; libtpw_unit_LinkPreviewBrace.dylib, tests/test_LinkPreviewBrace.py), main app:
//   src/LinkPreviewBrace.cpp   LinkPreviewBrace, LinkPreviewBraceToPylon and the globals of the copies they keep
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sCoasterInstance* gCoasterInstances;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
tag_sM3D2Header* gHeightfieldHeader;
s3DPoint gPointerWorldPosn;
CActionRecorder gActionRecorder;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float BitsFloat(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, sizeof(f));
    return f;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    ev("IsValid", {x, z, reinterpret_cast<intptr_t>(a), b, c, d, e});
    return static_cast<int32_t>(ret("IsValid"));
}

void LinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params, int32_t valid) { ev("LinkBrace", {lab(brace), lab(params), valid}); }

void LinkPylon(sPylonRecord* pylon, sCoasterTrackParams* params, uint32_t x, uint32_t z, uint32_t index) {
    ev("LinkPylon", {lab(pylon), lab(params), x, z, index});
}

void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("Insert", {lab(instance), lab(brace), lab(pylon), level, CanonBits(height), a, b, c, d});
    float value = BitsFloat(ret("InsertOut"));
    std::memcpy(out, &value, sizeof(value));
}

float GetNewBraceLoftInterval(sLinkPylon* pylon, sPylonControl* control, tag_sTrackBrace* brace, tag_sTrackBrace* other, int32_t a, int32_t b,
                              float* out) {
    ev("Loft", {lab(pylon), lab(control), lab(brace), lab(other), a, b, reinterpret_cast<intptr_t>(out)});
    return BitsFloat(ret("Loft"));
}

int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet) {
    ev("Level", {CanonBits(height), lab(pylon), quiet});
    *level = static_cast<uint32_t>(ret("LevelWord"));
    float value = BitsFloat(ret("LevelOut"));
    std::memcpy(out, &value, sizeof(value));
    return 1;
}

float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation) {
    ev("YOffset", {x, z, reinterpret_cast<intptr_t>(a), w, h, flags, rotation});
    return BitsFloat(ret("YOffset"));
}

void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) { ev("Flag", {lab(brace)}); }

void CorrectBraceNumbering(tag_sTrackBrace* brace) { ev("Number", {lab(brace)}); }

float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    ev("Cost", {lab(instance), a, lab(brace), reinterpret_cast<char*>(b) - reinterpret_cast<char*>(brace),
                reinterpret_cast<char*>(c) - reinterpret_cast<char*>(brace), reinterpret_cast<intptr_t>(d), reinterpret_cast<intptr_t>(e)});
    return BitsFloat(ret("Cost"));
}

void LLCC_SystemAddToSessionCost(int32_t cost) { ev("AddCost", {cost}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_lpb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"instances", &gCoasterInstances},
        {"word1790", &gCoasterWord1790},
        {"input", &gCoasterInputFlags},
        {"header", &gHeightfieldHeader},
        {"pointer", &gPointerWorldPosn},
        {"recording", &gActionRecorder.mRecording},
        {"saved_flag", &gLinkSavedFlag},
        {"saved_pylon", &gLinkSavedPylon},
        {"saved_braces", &gLinkSavedBraces},
        {"heights", &gLinkSavedHeights},
        {"found", &gLinkFoundPylon},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lpb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 field holds for a host address.
uint32_t tpw_lpb_off(uint64_t address) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)))); }

// The host offsets of the words the module writes by Mac offset, in the order of the module's LAYOUT_NAMES.
int32_t tpw_lpb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPylons));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mLastBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraceB5C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBrace64));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewPylon));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBrace6C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF118));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFE8w));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF90));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF74));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mIndex));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mCellZ));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mControl));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mX));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mZ));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mNext));
    out[n++] = static_cast<int64_t>(sizeof(sPylonRecord));
    out[n++] = static_cast<int64_t>(offsetof(sBraceSpline, mControlsA));
    return n;
}

// ---- the functions
void tpw_lpb_lpb(uint64_t instance) { LinkPreviewBrace(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }

void tpw_lpb_lpbp(uint64_t instance, uint64_t brace, uint64_t pylon, float f, int32_t copy) {
    LinkPreviewBraceToPylon(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)),
                            reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)),
                            reinterpret_cast<sPylonRecord*>(static_cast<uintptr_t>(pylon)), f, copy);
}

}  // extern "C"
