// Test library for the coaster editor's data change and loft toggle (unit v5054; libtpw_unit_HandleCoasterDataChange.dylib,
// tests/test_HandleCoasterDataChange.py), main app:
//   src/game/rides/CoasterEdit.cpp, src/game/rides/CoasterTrack.cpp   HandleCoasterDataChange, ToggleLoftPylonState, gCoasterVerticalFloor
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterInputFlags;
tag_sTrackBrace* gCoasterDragBrace;
float gCoasterDragAnchor[2];
float gCoasterDragScaleY;
float gCoasterDragLastDY;
float* gCoasterVerticalTarget1;
float* gCoasterVerticalTarget2;
float* gCoasterHorizontalTarget1;
float* gCoasterHorizontalTarget2;
float gCoasterVerticalBase1;
float gCoasterVerticalMin1;
float gCoasterVerticalMax1;
float gCoasterVerticalBase2;
float gCoasterCostBefore;
float gPointerScreenXY[2];
CWorld* gGameWorld;
CActionRecorder gActionRecorder;

namespace {

CBankAccount* gBank;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float RetFloat(const char* key) {
    uint32_t bits = static_cast<uint32_t>(ret(key));
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

}  // namespace

// ---- the fakes
float GetHeightForInterval(float interval, sPylonControl* pylon) {
    ev("GetHeightForInterval", {CanonBits(interval), lab(pylon)});
    return RetFloat("GetHeightForInterval");
}
void DoImmediatePhysicsFeedback(tag_sCoasterInstance* instance) { ev("DoImmediatePhysicsFeedback", {lab(instance)}); }
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }
void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("SetupBraceAnimFrameData", {lab(type), lab(brace)}); }
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("UpdatePylonStackMeshes", {lab(control), lab(pylon), a, b});
}
void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) { ev("FlagCollisionUpdateTrackAroundBrace", {lab(brace)}); }
void CActionRecorder::RecordAction(int32_t action, float value, uint16_t id) {
    ev("RecordAction", {lab(this), action, CanonBits(value), id});
}
float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    ev("CalculateBraceCost", {lab(instance), a, lab(brace), lab(b), lab(c), lab(d), lab(e)});
    return RetFloat("CalculateBraceCost");
}
float GetIntervalForHeight(float height, sPylonControl* pylon) {
    ev("GetIntervalForHeight", {CanonBits(height), lab(pylon)});
    return RetFloat("GetIntervalForHeight");
}
float GetIntervalForHeightDelta(float height, sPylonControl* pylon) {
    ev("GetIntervalForHeightDelta", {CanonBits(height), lab(pylon)});
    return RetFloat("GetIntervalForHeightDelta");
}
float CalculateBraceHeight(tag_sTrackBrace* brace) {
    ev("CalculateBraceHeight", {lab(brace)});
    return RetFloat("CalculateBraceHeight");
}
void LockPointedAtRide(int32_t flag) { ev("LockPointedAtRide", {flag}); }
int32_t RoundUp(float value) {
    ev("RoundUp", {CanonBits(value)});
    return static_cast<int32_t>(ret("RoundUp"));
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return gBank;
}
void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), amount}); }
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return 0;
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), amount}); }

extern "C" {

uint64_t tpw_hcdc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},
        {"input", &gCoasterInputFlags},
        {"drag", &gCoasterDragBrace},
        {"anchor", gCoasterDragAnchor},
        {"scaley", &gCoasterDragScaleY},
        {"lastdy", &gCoasterDragLastDY},
        {"vt1", &gCoasterVerticalTarget1},
        {"vt2", &gCoasterVerticalTarget2},
        {"ht1", &gCoasterHorizontalTarget1},
        {"ht2", &gCoasterHorizontalTarget2},
        {"base1", &gCoasterVerticalBase1},
        {"min1", &gCoasterVerticalMin1},
        {"max1", &gCoasterVerticalMax1},
        {"base2", &gCoasterVerticalBase2},
        {"cost", &gCoasterCostBefore},
        {"ptrxy", gPointerScreenXY},
        {"world", &gGameWorld},
        {"recorder", &gActionRecorder},
        {"floor", &gCoasterVerticalFloor},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hcdc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_hcdc_setbank(uint64_t bank) { gBank = reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(bank)); }

int32_t tpw_hcdc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sCoasterInstance, mFlags);
    out[n++] = offsetof(tag_sCoasterInstance, mType);
    out[n++] = offsetof(tag_sCoasterInstance, mF40);
    out[n++] = offsetof(tag_sCoasterInstance, mPreviewBrace);
    out[n++] = offsetof(tag_sCoasterInstance, mF118);
    out[n++] = offsetof(tag_sCoasterInstance, mF11C);
    out[n++] = sizeof(tag_sCoasterInstance);
    out[n++] = offsetof(tag_sTrackBrace, mNumber);
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mPrev);
    out[n++] = offsetof(tag_sTrackBrace, mNext);
    out[n++] = offsetof(tag_sTrackBrace, mFD8);
    out[n++] = offsetof(tag_sTrackBrace, mFE8w);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = sizeof(tag_sTrackBrace);
    out[n++] = offsetof(sCoasterPylon, mFlags);
    out[n++] = offsetof(sCoasterPylon, mControl);
    out[n++] = offsetof(sCoasterPylon, mCount);
    out[n++] = offsetof(sCoasterPylon, mBraces);
    out[n++] = offsetof(sPylonControl, mFlags);
    out[n++] = offsetof(sPylonControl, mF68);
    out[n++] = offsetof(sPylonControl, mF6C);
    out[n++] = sizeof(sPylonControl);
    out[n++] = offsetof(CActionRecorder, mRecording);
    out[n++] = sizeof(CActionRecorder);
    return n;
}

void tpw_hcdc_change(uint32_t flags, uint64_t brace, uint64_t instance, int32_t a, int32_t b) {
    HandleCoasterDataChange(flags, reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)),
                            reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)), a, b);
}

int32_t tpw_hcdc_toggle() { return ToggleLoftPylonState(); }

}  // extern "C"
