// Test library for the pylon edit states of the track editor (unit v5084; libtpw_unit_DisableCurrentPylonEditState.dylib,
// tests/test_DisableCurrentPylonEditState.py), main app:
//   src/DisableCurrentPylonEditState.cpp   ExchangeTrackBraces, ToggleSwapUpPylonState, ToggleSwapDownPylonState, SetTrackPreview2ndBrace,
//                                          DisableCurrentPylonEditState and the three words it clears
//
// The callees outside the unit are fakes that record what the real ones read; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance = nullptr;
int32_t gExtrudeTextureKind = 0;
uint32_t gCoasterWord16E0 = 0;
tag_sTrackBrace* gPylonMoveMark2 = nullptr;
tag_sTrackBrace* gPylonMoveMark1 = nullptr;
tag_sTrackBrace* gCoasterDragBrace = nullptr;
uint32_t gCoasterInputFlags = 0;
tag_sRideAnimSet* gRideAnimSets[8];
CActionRecorder gActionRecorder;

namespace {

const uint32_t kNone = 0xffffffffu;

uint32_t Word(const void* p) { return p != nullptr ? *static_cast<const uint32_t*>(p) : kNone; }

uint32_t BraceIndex(const tag_sTrackBrace* brace) {
    uint32_t value;
    std::memcpy(&value, brace->mF00 + 4, sizeof(value));
    return value;
}

// The count and the numbers of the first four braces of a pylon (kNone past the count or for a NULL slot).
struct PylonView {
    int64_t count, slot[4];
};

PylonView View(tag_sPylon* pylon) {
    sCoasterPylon* node = reinterpret_cast<sCoasterPylon*>(pylon);
    PylonView view;
    view.count = node->mCount;
    for (uint32_t i = 0; i < 4; ++i)
        view.slot[i] = i < node->mCount ? Word(node->mBraces[i]) : kNone;
    return view;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("Flag", {Word(instance), flag}); }

void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) { ev("Flash", {Word(instance), mode}); }

void LockPointedAtRide(int32_t flag) { ev("Lock", {flag}); }

void CActionRecorder::RecordAction(int32_t action, uint16_t value) { ev("Record", {action, value}); }

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) {
    ev("Setup", {Word(type), Word(brace), BraceIndex(brace), Word(&brace->mFE8w)});
}

void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon) {
    PylonView v = View(pylon);
    ev("Structs", {Word(control), Word(&pylon->mFlags), v.count, v.slot[0], v.slot[1], v.slot[2], v.slot[3]});
}

void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    PylonView v = View(pylon);
    ev("Meshes", {Word(control), Word(&pylon->mFlags), v.count, v.slot[0], v.slot[1], v.slot[2], v.slot[3], a, b});
}

int32_t ToggleLoftPylonState() {
    ev("Loft", {});
    return 0;
}

void ToggleRotatePylonState() { ev("Rotate", {}); }

void ToggleLoopTurnPylonState() { ev("LoopTurn", {}); }

void ToggleBankInclinePylonState() { ev("Bank", {}); }

extern "C" {

uint64_t tpw_cb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance}, {"extrude", &gExtrudeTextureKind}, {"w16e0", &gCoasterWord16E0},
        {"mark2", &gPylonMoveMark2},     {"mark1", &gPylonMoveMark1},       {"w1788", &gCoasterWord1788},
        {"drag", &gCoasterDragBrace},    {"w1790", &gCoasterWord1790},      {"iflags", &gCoasterInputFlags},
        {"w16dc", &gCoasterWord16DC},    {"rides", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a host address (the node block of a mesh header is read through one).
uint64_t tpw_cb_off(uint64_t address) { return static_cast<uint64_t>(tpw::ArenaOffset(reinterpret_cast<void*>(address))); }

int32_t tpw_cb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF40));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFE8w));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mControl));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterPylon, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    return n;
}

void tpw_cb_exchange(uint64_t a, uint64_t b) {
    ExchangeTrackBraces(reinterpret_cast<tag_sTrackBrace*>(a), reinterpret_cast<tag_sTrackBrace*>(b));
}
void tpw_cb_up() { ToggleSwapUpPylonState(); }
void tpw_cb_down() { ToggleSwapDownPylonState(); }
void tpw_cb_preview(uint64_t brace, int32_t flag) { SetTrackPreview2ndBrace(reinterpret_cast<tag_sTrackBrace*>(brace), flag); }
void tpw_cb_disable() { DisableCurrentPylonEditState(); }

}  // extern "C"
