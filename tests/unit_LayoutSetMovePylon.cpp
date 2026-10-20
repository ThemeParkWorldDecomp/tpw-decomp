// Test library for the editor coaster's layout commands (unit v5261; libtpw_unit_LayoutSetMovePylon.dylib, tests/test_LayoutSetMovePylon.py):
//   src/TPConfig_LayoutSetMovePylon.cpp   LayoutStackPylonUp/Down, LayoutDeleteMultiPylons, LayoutSetMovePylon, LayoutRegenerateTrack x2,
//                                         LayoutSetCoasterEditOld, LayoutSetCoasterEdit
//
// The callees are fakes that record the arguments (objects as their index in the test's registry, -2 for NULL, -1 for unknown) and answer
// from the queues. The globals of other files the unit uses are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

void DoDeleteMultiPylons();
void SetCoasterEdit(void* coaster, int32_t flag);
void LayoutStackPylonUp(uint16_t id);
void LayoutStackPylonDown(uint16_t id);
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b);
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c);
void LayoutRegenerateTrack();
void LayoutRegenerateTrack(uint32_t id);
void LayoutSetCoasterEditOld(uint32_t entrance);
void LayoutSetCoasterEdit(uint32_t id);

tag_sCoasterInstance* gCoasterCurrent;
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterEditInstance;
void* gObjectTable[0x100];
uint32_t gCoasterWord16E0;
tag_sTrackBrace* gCoasterDragBrace;

namespace {

void* gObj[16];

int64_t Idx(const void* p) {
    if (p == nullptr) return -2;
    for (int i = 0; i < 16; i++)
        if (gObj[i] == p) return i;
    return -1;
}

void* Obj(int64_t i) { return i >= 0 && i < 16 ? gObj[i] : nullptr; }

}  // namespace

// ---- the fakes
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }

tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id) {
    ev("GetTrackBrace", {Idx(params), static_cast<int64_t>(id)});
    return static_cast<tag_sTrackBrace*>(Obj(ret("GetTrackBrace")));
}

bool StartMovePylon(tag_sTrackBrace* brace) {
    ev("StartMovePylon", {Idx(brace)});
    return ret("StartMovePylon") != 0;
}

void DoDeleteMultiPylons() { ev("DoDeleteMultiPylons", {Idx(gCoasterDragBrace)}); }

void GenerateTrack(tag_sCoasterInstance* instance) { ev("GenerateTrack", {Idx(instance)}); }

void ExchangeTrackBraces(tag_sTrackBrace* first, tag_sTrackBrace* second) { ev("ExchangeTrackBraces", {Idx(first), Idx(second)}); }

tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    ev("FindCoasterInstanceFromEntranceID", {static_cast<int64_t>(entrance)});
    return static_cast<tag_sCoasterInstance*>(Obj(ret("FindCoasterInstanceFromEntranceID")));
}

void SetCoasterEdit(void* coaster, int32_t flag) { ev("SetCoasterEdit", {Idx(coaster), flag}); }

extern "C" {

uint64_t tpw_lmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},      {"drag", &gCoasterDragBrace}, {"w16e0", &gCoasterWord16E0},
        {"current", &gCoasterCurrent},        {"currentdata", &gCoasterCurrentData}, {"table", gObjectTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Registers object i; index 0 is the edit instance (its +0x3c block is registered as 6).
void tpw_lmp_reg(int32_t i, uint64_t address) {
    gObj[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    if (i == 0) gObj[6] = &static_cast<tag_sCoasterInstance*>(gObj[0])->mBlock3C;
}

uint64_t tpw_lmp_block(uint64_t instance) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))->mBlock3C));
}

int32_t tpw_lmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_lmp_up(uint32_t id) { LayoutStackPylonUp(static_cast<uint16_t>(id)); }
void tpw_lmp_down(uint32_t id) { LayoutStackPylonDown(static_cast<uint16_t>(id)); }
void tpw_lmp_delete(uint32_t a, uint32_t b) { LayoutDeleteMultiPylons(static_cast<uint16_t>(a), static_cast<uint16_t>(b)); }
void tpw_lmp_move(uint32_t a, uint32_t b, uint32_t c) {
    LayoutSetMovePylon(static_cast<uint16_t>(a), static_cast<uint16_t>(b), static_cast<uint16_t>(c));
}
void tpw_lmp_regen() { LayoutRegenerateTrack(); }
void tpw_lmp_regen_id(uint32_t id) { LayoutRegenerateTrack(id); }
void tpw_lmp_edit_old(uint32_t entrance) { LayoutSetCoasterEditOld(entrance); }
void tpw_lmp_edit(uint32_t id) { LayoutSetCoasterEdit(id); }

}  // extern "C"
