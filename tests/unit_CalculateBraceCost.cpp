// Test library for CalculateBraceCost 0x10058924, SetCoasterEdit(tag_sCoasterInstance*, int) 0x100585f4 and CoasterAcceptLeftMouseClick 0x10057f80
// (unit v5943; libtpw_unit_CalculateBraceCost.dylib, tests/test_CalculateBraceCost.py), main app:
//   src/TPConfig_LayoutSetMovePylon_SetCoasterEdit_CalculateBraceCost.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other files the
// unit reads or writes are defined here (zero at start like the Mac's bss).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "ui_main.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
uint32_t gCoasterWord16E0;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterInputFlags;
int32_t gRideHighlightKeep;
tag_sRideAnimSet* gRideAnimSets[32];
CActionRecorder gActionRecorder;

namespace {
uint64_t gLabelAddr[64];   // label number -> host address, written by the test (for the pointers the fakes answer)

float FloatOfBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
void* AddrOfLabel(int64_t label) { return label > 0 && label < 64 ? reinterpret_cast<void*>(static_cast<uintptr_t>(gLabelAddr[label])) : nullptr; }
}  // namespace

void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag);   // defined in the unit's source (not in a header)

// ---- the fakes
void CActionRecorder::RecordAction(int32_t action, uint16_t value) { ev("RecordAction", {action, value}); }
void CObject::CloseObject() { ev("CloseObject", {lab(this)}); }
void CObject::Reopen() { ev("Reopen", {lab(this)}); }
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) { ev("SetCoasterState", {lab(instance), static_cast<int64_t>(state)}); }
void SetCurrentEditCoaster(tag_sCoasterInstance* instance) {
    ev("SetCurrentEditCoaster", {lab(instance)});
    gCoasterEditInstance = instance;
}
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) {
    ev("FlagGenerateTrack", {lab(instance), flag});
    int64_t swap = ret("FlagSwap");
    if (swap != 0) gCoasterEditInstance = static_cast<tag_sCoasterInstance*>(AddrOfLabel(swap));
}
float GetHeightForInterval(float interval, sPylonControl* pylon) {
    ev("GetHeightForInterval", {fbits(interval), lab(pylon)});
    return FloatOfBits(ret("Height"));
}
tag_sRideAnimSet* GetPointedAtRide() {
    ev("GetPointedAtRide", {});
    return static_cast<tag_sRideAnimSet*>(AddrOfLabel(ret("Ride")));
}
bool StartMovePylon(tag_sTrackBrace* brace) {
    ev("StartMovePylon", {lab(brace)});
    return ret("StartMovePylon") != 0;
}
void ToggleLoopTurnPylonState() { ev("ToggleLoopTurnPylonState", {}); }
void ToggleBankInclinePylonState() { ev("ToggleBankInclinePylonState", {}); }
int32_t ToggleLoftPylonState() {
    ev("ToggleLoftPylonState", {});
    return 1;
}
void ToggleRotatePylonState() { ev("ToggleRotatePylonState", {}); }
void ToggleSwapUpPylonState() { ev("ToggleSwapUpPylonState", {}); }
void ToggleSwapDownPylonState() { ev("ToggleSwapDownPylonState", {}); }
void DoDeleteMultiPylons() { ev("DoDeleteMultiPylons", {}); }

extern "C" {

uint64_t tpw_cbc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},   {"enabled", &gCoasterEditEnabled}, {"down", &gCoasterEditDown},
        {"w16e0", &gCoasterWord16E0},      {"drag", &gCoasterDragBrace},      {"iflags", &gCoasterInputFlags},
        {"keep", &gRideHighlightKeep},     {"rides", &gRideAnimSets[0]},      {"rec", &gActionRecorder.mRecording},
        {"labels", &gLabelAddr[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cbc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets, in the order of NAMES in the test.
int32_t tpw_cbc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sCoasterInstance, mObjectInstanceID);
    out[n++] = offsetof(tag_sCoasterInstance, mType);
    out[n++] = offsetof(tag_sCoasterInstance, mState);
    out[n++] = offsetof(tag_sCoasterInstance, mCObject);
    out[n++] = offsetof(tag_sCoasterInstance, mBlock3C);
    out[n++] = offsetof(tag_sCoasterInstance, mF40);
    out[n++] = offsetof(tag_sCoasterInstance, mLastBrace);
    out[n++] = offsetof(tag_sCoasterInstance, mPylons);
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mNext);
    out[n++] = offsetof(tag_sTrackBrace, mX4C);
    out[n++] = offsetof(tag_sTrackBrace, mZ54);
    out[n++] = offsetof(tag_sTrackBrace, mFE8w);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = offsetof(tag_sTrackBrace, mWord70);
    out[n++] = offsetof(sTrackBraceData, mFlags);
    out[n++] = offsetof(sLinkPylon, mControl);
    out[n++] = offsetof(sLinkPylon, mOther);
    out[n++] = offsetof(sPylonControl, mFlags);
    out[n++] = offsetof(sPylonControl, mF5C);
    out[n++] = offsetof(sPylonControl, mF60);
    out[n++] = offsetof(sPylonControl, mF64);
    out[n++] = offsetof(tag_sCoasterType, mPylonsA);
    out[n++] = offsetof(sCoasterPylon, mFlags);
    out[n++] = offsetof(sCoasterPylon, mNext);
    out[n++] = offsetof(sCoasterPylon, mCount);
    out[n++] = offsetof(sCoasterPylon, mBraces);
    out[n++] = sizeof(void*);
    out[n++] = sizeof(tag_sCoasterInstance);
    out[n++] = sizeof(tag_sTrackBrace);
    out[n++] = sizeof(tag_sCoasterType);
    out[n++] = sizeof(sLinkPylon);
    out[n++] = sizeof(sCoasterPylon);
    return n;
}

uint32_t tpw_cbc_calc(uint64_t instance, int32_t a, uint64_t brace, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    float r = CalculateBraceCost(reinterpret_cast<tag_sCoasterInstance*>(instance), a, reinterpret_cast<tag_sTrackBrace*>(brace),
                                 reinterpret_cast<float*>(b), reinterpret_cast<float*>(c), reinterpret_cast<float*>(d), reinterpret_cast<float*>(e));
    uint32_t bits;
    std::memcpy(&bits, &r, 4);
    return bits;
}

void tpw_cbc_sce(uint64_t instance, int32_t flag) { SetCoasterEdit(reinterpret_cast<tag_sCoasterInstance*>(instance), flag); }

uint32_t tpw_cbc_cal(int32_t down) {
    CoasterAcceptLeftMouseClick(down);
    return 0;
}

}  // extern "C"
