// Test library for the editor's pylon placement (unit v5190; libtpw_unit_SetPotentialPylonArea.dylib, tests/test_SetPotentialPylonArea.py), main app:
//   src/TPCoaster_trains_SetPotentialPylonArea.cpp   SetPotentialPylonArea, PlacePylon
//
// The callees outside the unit (NewPylon, GetNewBraceLoftInterval, CTRACE, DetermineLinkLevel, NewTrackBrace, SetupBraceAnimFrameData,
// RecalcTrackParams, CalculateBraceCost, FloatToCell, the bank account, RejoinTrack, SetCoasterEditState, CoasterPanelEndTrack, UIHandleEndLoop,
// UIHandleMidLoop) are recording fakes answering from the test's queues; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

uint32_t gCoasterWord6890;
uint32_t gCoasterWord6894;
uint32_t gCoasterWord6898;
uint32_t gCoasterWord689C;
tag_sCoasterInstance* gCoasterEditInstance;
tag_sCoasterInstance* gCoasterInstances;
tag_sTrackBrace* gPylonMoveBrace;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
int32_t gHeightfieldYStart;
int32_t gHeightfieldXStart;
tag_sM3D2Header* gHeightfieldHeader;
CWorld* gGameWorld;
CActionRecorder gActionRecorder;

namespace {

void* gResult[3];  // 0 NewPylon's answer, 1 NewTrackBrace's, 2 the bank account

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

// ---- the fakes
sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    ev("NewPylon", {static_cast<int64_t>(x), static_cast<int64_t>(z), static_cast<int64_t>(flags), static_cast<int64_t>(unused), lab(control),
                    lab(point)});
    return static_cast<sPylonRecord*>(gResult[0]);
}

float GetNewBraceLoftInterval(sLinkPylon* pylon, sPylonControl* control, tag_sTrackBrace* brace, tag_sTrackBrace* other, int32_t a, int32_t b,
                              float* out) {
    ev("GetNewBraceLoftInterval", {lab(pylon), lab(control), lab(brace), lab(other), static_cast<int64_t>(a), static_cast<int64_t>(b)});
    *out = FromBits(ret("loft", 0));
    return 0.0f;
}

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t length = 0, sum = 0;
    for (const char* p = format; *p != 0; ++p) {
        ++length;
        sum += static_cast<unsigned char>(*p);
    }
    ev("CTRACE", {static_cast<int64_t>(level), length, sum});
}

int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet) {
    ev("DetermineLinkLevel", {CanonBits(height), lab(pylon), static_cast<int64_t>(quiet)});
    *level = static_cast<uint32_t>(ret("level", 0));
    *out = FromBits(ret("dlout", 0));
    return 1;
}

tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t isEnd, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra) {
    ev("NewTrackBrace", {lab(pylon), CanonBits(x), CanonBits(z), CanonBits(loft), static_cast<int64_t>(isEnd), lab(control),
                         static_cast<int64_t>(loops), static_cast<int64_t>(level), static_cast<int64_t>(extra)});
    return static_cast<tag_sTrackBrace*>(gResult[1]);
}

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("SetupBraceAnimFrameData", {lab(type), lab(brace)}); }

void RecalcTrackParams(tag_sCoasterInstance* instance) { ev("RecalcTrackParams", {lab(instance)}); }

float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    ev("CalculateBraceCost", {lab(instance), static_cast<int64_t>(a), lab(brace), CanonBits(*b), CanonBits(*c), lab(d), lab(e)});
    return FromBits(ret("cost", 0));
}

int32_t FloatToCell(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("FloatToCell", {static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xffffffffu)});
    return static_cast<int32_t>(ret("cell", 0));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(gResult[2]);
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("balance", 0));
}

void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), static_cast<int64_t>(amount)}); }

void RejoinTrack(tag_sCoasterInstance* instance) { ev("RejoinTrack", {lab(instance)}); }
void SetCoasterEditState(uint32_t state) { ev("SetCoasterEditState", {static_cast<int64_t>(state)}); }
void CoasterPanelEndTrack() { ev("CoasterPanelEndTrack", {}); }
void UIHandleEndLoop() { ev("UIHandleEndLoop", {}); }
void UIHandleMidLoop() { ev("UIHandleMidLoop", {}); }

extern "C" {

uint64_t tpw_spa_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w6890", &gCoasterWord6890},   {"w6894", &gCoasterWord6894},     {"w6898", &gCoasterWord6898},
        {"w689c", &gCoasterWord689C},   {"edit", &gCoasterEditInstance},  {"insts", &gCoasterInstances},
        {"pmove", &gPylonMoveBrace},    {"w1790", &gCoasterWord1790},     {"input", &gCoasterInputFlags},
        {"hh", &gHeightfieldHeight},    {"hw", &gHeightfieldWidth},       {"hys", &gHeightfieldYStart},
        {"hxs", &gHeightfieldXStart},   {"hdr", &gHeightfieldHeader},     {"world", &gGameWorld},
        {"rec", &gActionRecorder},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_spa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_spa_off(uint64_t p) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(p)))); }

void tpw_spa_setres(int32_t which, uint64_t p) { gResult[which] = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

// The host offsets of every field the module writes by Mac offset, in the order of the module's MAC table.
int32_t tpw_spa_layout(int64_t* out) {
    int32_t n = 0;
#define OFF(S, F) out[n++] = static_cast<int64_t>(offsetof(S, F))
    OFF(tag_sCoasterInstance, mFlags);
    OFF(tag_sCoasterInstance, mType);
    OFF(tag_sCoasterInstance, mNext);
    OFF(tag_sCoasterInstance, mPylons);
    OFF(tag_sCoasterInstance, mBlock3C);
    OFF(tag_sCoasterInstance, mBraceCount);
    OFF(tag_sCoasterInstance, mBraces);
    OFF(tag_sCoasterInstance, mBraceB5C);
    OFF(tag_sCoasterInstance, mPreviewBrace);
    OFF(tag_sCoasterInstance, mBrace64);
    OFF(tag_sTrackBrace, mNumber);
    OFF(tag_sTrackBrace, mFlags);
    OFF(tag_sTrackBrace, mPrev);
    OFF(tag_sTrackBrace, mNext);
    OFF(tag_sTrackBrace, mData);
    OFF(sTrackBraceData, mCellX);
    OFF(sTrackBraceData, mCellZ);
    OFF(sPylonRecord, mFlags);
    OFF(sPylonRecord, mIndex);
    OFF(sPylonRecord, mControl);
    OFF(sPylonRecord, mX);
    OFF(sPylonRecord, mZ);
    OFF(sPylonRecord, mNext);
    OFF(sPylonRecord, mCount);
    OFF(sPylonRecord, mBraces);
    OFF(tag_sCoasterType, mPylonsA);
    OFF(tag_sCoasterType, mPylonsB);
    OFF(sPylonControl, mFA0);
    OFF(tag_sM3D2Header, mHField);
    OFF(sHField, mF18);
    OFF(CActionRecorder, mRecording);
#undef OFF
    return n;
}

void tpw_spa_sppa() { SetPotentialPylonArea(); }

uint32_t tpw_spa_place(uint32_t x, uint32_t z, int32_t flag, uint64_t outPylon, uint64_t outBrace, uint32_t level, uint32_t extra) {
    return static_cast<uint32_t>(PlacePylon(x, z, flag, reinterpret_cast<tag_sPylon**>(static_cast<uintptr_t>(outPylon)),
                                            reinterpret_cast<tag_sTrackBrace**>(static_cast<uintptr_t>(outBrace)), static_cast<uint8_t>(level), extra));
}

}  // extern "C"
