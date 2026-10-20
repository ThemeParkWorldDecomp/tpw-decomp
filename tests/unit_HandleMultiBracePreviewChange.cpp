// Test library for HandleMultiBracePreviewChange (unit v5688; libtpw_unit_HandleMultiBracePreviewChange.dylib,
// tests/test_HandleMultiBracePreviewChange.py), main app:
//   src/TPCoaster_ticks_HandleMultiBracePreviewChange.cpp   HandleMultiBracePreviewChange
//
// The callees outside the unit are fakes that record the arguments (objects as the first word they hold, an id the test writes) and answer
// from the test's queues. The globals of other files the unit reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

// ---- globals of other files
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterWord16BC;
int32_t gPylonMoveZ;
int32_t gPylonMoveX;
float gPylonSecondCost;
float gCoasterCostBefore;
tag_sTrackBrace* gPylonMoveBrace;
tag_sTrackBrace* gPylonMoveMark2;
tag_sTrackBrace* gPylonMoveMark1;
uint32_t gCoasterWord1788;
tag_sTrackBrace* gCoasterStartBrace;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterInputFlags;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
CActionRecorder gActionRecorder;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// The first word of an object (the id the test wrote there), -1 for NULL.
int64_t Id(const void* p) { return p == nullptr ? -1 : static_cast<int64_t>(*static_cast<const int32_t*>(p)); }

int64_t NN(const void* p) { return p != nullptr ? 1 : 0; }

tag_sPylon* gPylonTable[16];
int32_t gPylonCount = 0;

}  // namespace

// ---- the fakes
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {Id(instance), static_cast<int64_t>(flag)}); }

void SetTrackPreview2ndBrace(tag_sTrackBrace* brace, int32_t flag) { ev("SetTrackPreview2ndBrace", {Id(brace), static_cast<int64_t>(flag)}); }

tag_sPylon* GetPylonUnderCursor(uint32_t* a, uint32_t* b, uint32_t* c) {
    ev("GetPylonUnderCursor", {NN(a), NN(b), NN(c)});
    int64_t idx = ret("GetPylonUnderCursor");
    if (idx < 0 || idx >= gPylonCount) return nullptr;
    return gPylonTable[idx];
}

void MoveBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sPylon* pylon, uint32_t x, uint32_t z, float* a, float* b, int32_t c,
               int32_t d) {
    ev("MoveBrace", {Id(instance), Id(brace), Id(pylon), static_cast<int64_t>(x), static_cast<int64_t>(z), NN(a), NN(b), static_cast<int64_t>(c),
                     static_cast<int64_t>(d)});
}

float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    ev("CalculateBraceCost", {Id(instance), static_cast<int64_t>(a), Id(brace), NN(b), NN(c), NN(d), NN(e)});
    uint32_t bits = static_cast<uint32_t>(ret("CalculateBraceCost"));
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

int32_t RoundUp(float value) {
    ev("RoundUp", {CanonBits(value)});
    return static_cast<int32_t>(ret("RoundUp"));
}

int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    ev("IsValidPylonPosition", {static_cast<int64_t>(x), static_cast<int64_t>(z), NN(a), static_cast<int64_t>(b), static_cast<int64_t>(c),
                                static_cast<int64_t>(d), static_cast<int64_t>(e)});
    return static_cast<int32_t>(ret("IsValidPylonPosition"));
}

void LLCC_SystemAddToSessionCost(int32_t cost) { ev("LLCC_SystemAddToSessionCost", {static_cast<int64_t>(cost)}); }

extern "C" {

uint64_t tpw_hmbpc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},   {"w16bc", &gCoasterWord16BC},         {"movez", &gPylonMoveZ},
        {"movex", &gPylonMoveX},           {"second", &gPylonSecondCost},        {"costbefore", &gCoasterCostBefore},
        {"movebrace", &gPylonMoveBrace},   {"mark2", &gPylonMoveMark2},          {"mark1", &gPylonMoveMark1},
        {"w1788", &gCoasterWord1788},      {"start", &gCoasterStartBrace},       {"drag", &gCoasterDragBrace},
        {"inflags", &gCoasterInputFlags},  {"rides", &gRideAnimSets[0]},         {"rec", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hmbpc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The pylon records GetPylonUnderCursor answers by index.
void tpw_hmbpc_setpylons(const uint64_t* addrs, int32_t n) {
    gPylonCount = n;
    for (int32_t i = 0; i < n && i < 16; i++) gPylonTable[i] = reinterpret_cast<tag_sPylon*>(static_cast<uintptr_t>(addrs[i]));
}

// The host offsets of the fields the module writes by Mac offset (the order of the module's NAMES).
int32_t tpw_hmbpc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mLastBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPylons));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mNext));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNumber));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mCellZ));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF14));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF18Table));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF30));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFCC));
    return n;
}

void tpw_hmbpc_call(uint64_t instance, uint64_t previous, uint64_t pointed, int32_t changed, uint32_t x, uint32_t z) {
    HandleMultiBracePreviewChange(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)),
                                  reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(previous)),
                                  reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(pointed)), changed, x, z);
}

}  // extern "C"
