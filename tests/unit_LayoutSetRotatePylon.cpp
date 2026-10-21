// Test library for the pylon setters of the editor's coaster (unit v5039; libtpw_unit_LayoutSetRotatePylon.dylib, tests/test_LayoutSetRotatePylon.py), main app:
//   src/game/rides/CoasterEdit.cpp   SetLoftPylon, LayoutSetLoftPylon, SetRotatePylon, LayoutSetRotatePylon, SetWobblePylon, LayoutSetWobblePylon,
//                                  ToggleDrawUpSpline, CorrectBraceNumbering; defines gDrawUpSplineFlag
//
// GetTrackBrace, HandleCoasterDataChange and FlagGenerateTrack are recording fakes; the brace GetTrackBrace answers is set by the test.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "keytable_actions.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterEditInstance;
static uint64_t gTestBrace;

tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id) {
    int64_t rel = reinterpret_cast<char*>(params) - reinterpret_cast<char*>(gCoasterEditInstance) -
                  static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C)) + 0x3c;
    ev("GetTrackBrace", {rel, static_cast<int64_t>(id)});
    return reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(gTestBrace));
}

void HandleCoasterDataChange(uint32_t flags, tag_sTrackBrace* brace, tag_sCoasterInstance* instance, int32_t a, int32_t b) {
    ev("HandleCoasterDataChange", {static_cast<int64_t>(flags), brace == reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(gTestBrace)) ? 1 : 0,
                                   instance == gCoasterEditInstance ? 1 : 0, a, b});
}

void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) {
    ev("FlagGenerateTrack", {instance == gCoasterEditInstance ? 1 : 0, flag});
}

extern "C" {

uint64_t tpw_v5039_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"inst", &gCoasterEditInstance},
        {"draw", &gDrawUpSplineFlag},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5039_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_v5039_set_brace(uint64_t brace) { gTestBrace = brace; }

int32_t tpw_v5039_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNumber));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF6C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mAngle));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFD8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFDC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFE8w));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFF8));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    return n;
}

void tpw_v5039_set_loft(float v, uint32_t id, int32_t a, int32_t b) { SetLoftPylon(v, id, a, b); }
void tpw_v5039_lset_loft(float v, uint32_t id) { LayoutSetLoftPylon(v, id); }
void tpw_v5039_set_rot(float v, uint32_t id, int32_t a, int32_t b) { SetRotatePylon(v, id, a, b); }
void tpw_v5039_lset_rot(float v, uint32_t id) { LayoutSetRotatePylon(v, id); }
void tpw_v5039_set_wob(float x, float z, uint32_t id, int32_t a, int32_t b) { SetWobblePylon(x, z, id, a, b); }
void tpw_v5039_lset_wob(float x, float z, uint32_t id) { LayoutSetWobblePylon(x, z, id); }
int32_t tpw_v5039_toggle() { return ToggleDrawUpSpline(); }
void tpw_v5039_correct(uint64_t brace) { CorrectBraceNumbering(reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace))); }

}  // extern "C"
