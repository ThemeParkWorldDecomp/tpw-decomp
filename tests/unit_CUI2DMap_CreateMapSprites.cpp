// Test library for the 2D map's sprite maker (unit v5058; libtpw_unit_CUI2DMap_CreateMapSprites.dylib, tests/test_CUI2DMap_CreateMapSprites.py):
//   src/game/ui/ui_2dmap.cpp   CUI2DMap::CreateMapSprites
//
// The callees outside the unit are fakes that record what the real ones read: the world's two thing list walkers (answers from the queues
// "Head" and "Next"), the sprite constructor, UpdateSpritePosition and GAME_ASSERTMSG. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
uint16_t gThingIdNone;
uint16_t gObjectIdNone;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
// gMainBalance is the real class on the program; here raw bytes under the same symbol (the unit reads words of it by offset)
alignas(16) uint8_t gBalanceStore[0x800] __asm__("_gMainBalance");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("Head", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("Head"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("Next", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("Next"));
    return r;
}

CUI2DMapSprite::CUI2DMapSprite(void* image, EUI2DMapSpriteID id, TUI2DMapIndividualShowStatus status, float scale) {
    std::memset(this, 0, sizeof(*this));
    ev("Ctor", {newlab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(image)), static_cast<int64_t>(id), status.mA, status.mB, fbits(scale)});
}

void CUI2DMap::UpdateSpritePosition(CUI2DMapSprite* sprite) {
    ev("Update", {lab(this), lab(sprite), CanonBits(sprite->mX), CanonBits(sprite->mY), lab(sprite->mThing)});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    (void)format;
    ev("Assert", {condition & 0xff});
}

extern "C" {

uint64_t tpw_cm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"none", &gThingIdNone}, {"objnone", &gObjectIdNone}, {"world", &gGameWorld}, {"tarr", &gThingArray[0]}, {"bal", gBalanceStore},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mX));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mY));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    return n;
}

void tpw_cm_run(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->CreateMapSprites(); }

}  // extern "C"
