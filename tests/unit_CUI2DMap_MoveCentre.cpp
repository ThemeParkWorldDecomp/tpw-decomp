// Test library for the 2D map's MoveCentre, InitSprites and UpdateMapZoom (unit v5083; libtpw_unit_CUI2DMap_MoveCentre.dylib,
// tests/test_CUI2DMap_MoveCentre.py), main app:
//   src/game/ui/ui_2dmap.cpp   CUI2DMap::MoveCentre, InitSprites, UpdateMapZoom
//
// The callees outside the unit are recording fakes: CWorld::GetUsedThingHead / GetNextUsedThing, GAME_ASSERTMSG, CEZSprite::Init /
// Enable / LoadSprite, CMapZoom::Use / Window, CUI2DMap::CreateMapSprites and UpdateAllSpritePositions. The globals of other files the
// unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ui_main.h"
#include "World.h"
#include "TPMain.h"
#include "Debug.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gScreenWidth;
int32_t gScreenHeight;
CUI2DMapImageEntry gUI2DMapImages[6];
uint16_t gUI2DMapThingListEnd;
uint16_t gUI2DMapObjectListEnd;
CEZSprite gei_EZSprite;
CMapZoom gei_MapZoom;

CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}
CMapZoom::CMapZoom() {}
CMapZoom::~CMapZoom() {}

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// A text as one number: the sum of each byte times its position (1 based).
int64_t TextHash(const char* text) {
    int64_t sum = 0;
    for (int64_t i = 0; text[i] != 0; i++) sum += (i + 1) * static_cast<uint8_t>(text[i]);
    return sum;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head"));
    return result;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), static_cast<int64_t>(*reinterpret_cast<const uint16_t*>(thing))});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next"));
    return result;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<int64_t>(static_cast<uint32_t>(condition)), TextHash(format)});
}

int32_t CEZSprite::Init(int32_t maxInstances, int32_t maxSprites) {
    ev("Init", {lab(this), maxInstances, maxSprites});
    return 0;
}

int32_t CEZSprite::Enable() {
    ev("Enable", {lab(this)});
    return 0;
}

SSpriteEngineData* CEZSprite::LoadSprite(char* name) {
    ev("LoadSprite", {lab(this), TextHash(name)});
    return reinterpret_cast<SSpriteEngineData*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("LoadSprite"))));
}

void CMapZoom::Use(float x0, float y0, float x1, float y1) {
    ev("Use", {lab(this), CanonBits(x0), CanonBits(y0), CanonBits(x1), CanonBits(y1)});
}

void CMapZoom::Window(float x0, float y0, float x1, float y1) {
    ev("Window", {lab(this), CanonBits(x0), CanonBits(y0), CanonBits(x1), CanonBits(y1)});
}

void CUI2DMap::CreateMapSprites() { ev("CreateMapSprites", {lab(this)}); }

void CUI2DMap::UpdateAllSpritePositions() { ev("UpdateAllSpritePositions", {lab(this)}); }

extern "C" {

uint64_t tpw_u5083_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ezsprite", &gei_EZSprite},
        {"mapzoom", &gei_MapZoom},
        {"gGameWorld", &gGameWorld},
        {"thingarr", &gThingArray[0]},
        {"images", &gUI2DMapImages[0]},
        {"thingend", &gUI2DMapThingListEnd},
        {"objend", &gUI2DMapObjectListEnd},
        {"screenw", &gScreenWidth},
        {"screenh", &gScreenHeight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u5083_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_u5083_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapImageEntry));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapImageEntry, mName));
    return n;
}

void tpw_u5083_movecentre(uint64_t map, int32_t direction, float amount) {
    reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->MoveCentre(static_cast<EUI2DMapMoveDirection>(direction), amount);
}

void tpw_u5083_initsprites(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->InitSprites(); }

void tpw_u5083_updatemapzoom(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->UpdateMapZoom(); }

}  // extern "C"
