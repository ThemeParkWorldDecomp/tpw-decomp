// Test library for the pylon drag of the track editor (unit v5026; libtpw_unit_DoMovePylon.dylib, tests/test_DoMovePylon.py), main app:
//   src/game/rides/CoasterEdit.cpp   StartMovePylon, DoMovePylon
//
// The callees outside the unit are recording fakes that answer from the test's queues. The globals of other files the unit reads are
// defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "World.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterEditInstance;
int32_t gExtrudeTextureKind;
uint32_t gCoasterWord16E0;
float gCoasterCostBefore;
uint32_t gCoasterInputFlags;
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
uint32_t gCoasterWord16BC;
int32_t gPylonMoveZ;
int32_t gPylonMoveX;
float gPylonSecondCost;
tag_sPylon* gPylonMoveOrigData;
float gPylonMoveHeight;
tag_sTrackBrace* gPylonMoveBrace;
tag_sTrackBrace* gPylonMoveMark2;
tag_sTrackBrace* gPylonMoveMark1;

namespace {

void* gBlocks[32];

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t U(uint32_t v) { return static_cast<int64_t>(v); }

void* BlockOf(int64_t label) { return label > 0 && label < 32 ? gBlocks[label] : nullptr; }

float BitsFloat(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), U(flag)}); }

tag_sPylon* GetPylonAtCell(uint32_t x, uint32_t z, uint32_t* out) {
    ev("GetPylonAtCell", {U(x), U(z), out ? 1 : 0});
    return static_cast<tag_sPylon*>(BlockOf(ret("GetPylonAtCell")));
}

void MoveBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sPylon* pylon, uint32_t x, uint32_t z, float* a, float* b, int32_t c,
               int32_t d) {
    ev("MoveBrace", {lab(instance), lab(brace), lab(pylon), U(x), U(z), a ? 1 : 0, b ? 1 : 0, U(c), U(d)});
    if (a) *a = BitsFloat(ret("MoveBrace_a"));
    if (b) *b = BitsFloat(ret("MoveBrace_b"));
}

float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* c, float* d, float* e, float* f) {
    ev("CalculateBraceCost", {lab(instance), U(a), lab(brace), c ? 1 : 0, c ? CanonBits(*c) : 0, d ? 1 : 0, d ? CanonBits(*d) : 0, e ? 1 : 0,
                              e ? CanonBits(*e) : 0, f ? 1 : 0, f ? CanonBits(*f) : 0});
    return BitsFloat(ret("CalculateBraceCost"));
}

int32_t RoundUp(float value) {
    ev("RoundUp", {CanonBits(value)});
    return static_cast<int32_t>(ret("RoundUp"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(BlockOf(7));
}

void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), U(amount)}); }

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return 0;
}

void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), U(amount)}); }

void LLCC_AbortPylonMove() { ev("LLCC_AbortPylonMove", {}); }
void LLCC_ResetPylonMove() { ev("LLCC_ResetPylonMove", {}); }
void LLCC_StartPylonMove(int32_t x, int32_t z, int32_t flag) { ev("LLCC_StartPylonMove", {U(x), U(z), U(flag)}); }
void LockPointedAtRide(int32_t flag) { ev("LockPointedAtRide", {U(flag)}); }
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }
void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    ev("Assign3DObjectToAIWorld", {lab(instance), U(a), U(b), U(c), U(d)});
}
float CalculateBraceHeight(tag_sTrackBrace* brace) {
    ev("CalculateBraceHeight", {lab(brace)});
    return BitsFloat(ret("CalculateBraceHeight"));
}
void SetTrackPreview2ndBrace(tag_sTrackBrace* brace, int32_t flag) { ev("SetTrackPreview2ndBrace", {lab(brace), U(flag)}); }
void SetPotentialPylonArea() { ev("SetPotentialPylonArea", {}); }
void CActionRecorder::RecordAction(int32_t action, uint16_t a, uint16_t b, uint16_t c) {
    ev("RecordAction", {lab(this), U(action), U(a), U(b), U(c)});
}

extern "C" {

uint64_t tpw_dmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w16bc", &gCoasterWord16BC}, {"pz", &gPylonMoveZ}, {"px", &gPylonMoveX}, {"second", &gPylonSecondCost},
        {"orig", &gPylonMoveOrigData}, {"hgt", &gPylonMoveHeight}, {"mbrace", &gPylonMoveBrace}, {"mark2", &gPylonMoveMark2},
        {"mark1", &gPylonMoveMark1}, {"edit", &gCoasterEditInstance}, {"ext", &gExtrudeTextureKind}, {"e16e0", &gCoasterWord16E0},
        {"cost", &gCoasterCostBefore}, {"flags", &gCoasterInputFlags}, {"hh", &gHeightfieldHeight}, {"hw", &gHeightfieldWidth},
        {"world", &gGameWorld}, {"rec", &gActionRecorder},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_dmp_setblock(int32_t label, uint64_t address) {
    if (label > 0 && label < 32) gBlocks[label] = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

// The host offsets of the fields the module writes (LAYOUT_NAMES order).
int32_t tpw_dmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mCellZ));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF14));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF18));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF30));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mOwner));
    out[n++] = static_cast<int64_t>(offsetof(sPylonOwner, mF70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    return n;
}

uint32_t tpw_dmp_start(uint64_t brace) {
    return StartMovePylon(reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace))) ? 1u : 0u;
}

uint32_t tpw_dmp_do(uint32_t x, uint32_t z) { return DoMovePylon(x, z); }

}  // extern "C"
