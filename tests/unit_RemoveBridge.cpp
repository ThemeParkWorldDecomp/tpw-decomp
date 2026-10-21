// Test library for RemoveBridge and LLCC_SystemGetPointedPeepThing (unit v5689; libtpw_unit_RemoveBridge.dylib, tests/test_RemoveBridge.py), main app:
//   src/LLCellSystem_hoardimage_RemoveBridge.cpp
//
// The callees are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "Debug.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "sprite_engine.h"
#include "tb_resource_display.h"
#include "ui_main.h"

using namespace calllog;

int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
int32_t gLLCCE034;
int32_t gLLCCE038;
uint16_t gLLCCThingListEnd;
CIntrface gIntrface;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
SSpriteGameData* gHotSprite;
float gSpriteHotX;
float gSpriteHotY;
const MapDelta gLLCCPointedOffsets[9] = {{0, 0, 0}, {0, -1, 0}, {1, -1, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-1, 1, 0}, {-1, 0, 0}, {-1, -1, 0}};

namespace {
uint8_t gFakeCells[3][64];
uint8_t gFakeSprite[16];
const char* const kTexts[] = {"Found sprite %lx\n", " Found script %lx\n", "Thing has been allocated in ThingArray index 0 - shouldn't happen\n",
                              "  Checking against scripts %lx\n"};
int64_t TextId(const char* s) {
    for (int i = 0; i < 4; ++i)
        if (std::strcmp(kTexts[i], s) == 0) return i + 1;
    return -1;
}
CTrackCell* Cell(int64_t k) {
    if (k == 1) return reinterpret_cast<CTrackCell*>(gFakeCells[1]);
    if (k == 2) return reinterpret_cast<CTrackCell*>(gFakeCells[2]);
    if (k == 3) return reinterpret_cast<CTrackCell*>(gFakeCells[0]);
    return nullptr;
}
}  // namespace

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCell", {lab(this), delta.mX, delta.mY, offset});
    return Cell(ret("nb"));
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return x >= 0 && x < 128 && y >= 0 && y < 128;
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {lab(this)});
    return static_cast<uint8_t>(ret("dir"));
}
void CTrackCell::UnSetFlag(uint16_t flags) { ev("UnSetFlag", {lab(this), flags}); }
void CTrackCell::SetFlag(uint16_t flags) { ev("SetFlag", {lab(this), flags}); }
int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {lab(this)});
    return static_cast<int32_t>(ret("bridge"));
}
int32_t CTrackCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {lab(this), flag});
    return static_cast<int32_t>(ret("isflag"));
}
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {lab(this)}); }

void UI_GetMouseXY(int16_t& x, int16_t& y) {
    ev("UI_GetMouseXY", {1});
    x = static_cast<int16_t>(ret("mx"));
    y = static_cast<int16_t>(ret("my"));
}
int32_t SCR_WhichScriptHasThisSprite(void* sprite) {
    ev("SCR_WhichScriptHasThisSprite", {lab(sprite)});
    return static_cast<int32_t>(ret("script"));
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int64_t id = TextId(format);
    if (id == 1) {
        void* p = va_arg(ap, void*);
        ev("CTRACE", {level, id, lab(p)});
    } else {
        int32_t v = va_arg(ap, int32_t);
        ev("CTRACE", {level, id, v});
    }
    va_end(ap);
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition, TextId(format)}); }

extern "C" {

uint64_t tpw_rb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"firstx", &gLLCCFirstBlockX}, {"firsty", &gLLCCFirstBlockY}, {"e034", &gLLCCE034},   {"e038", &gLLCCE038},
        {"end", &gLLCCThingListEnd},   {"f24", &gIntrface.mF24},     {"thingarr", gThingArray}, {"world", &gGameWorld},
        {"hotx", &gSpriteHotX},        {"hoty", &gSpriteHotY},       {"hotspr", &gHotSprite},  {"c0", gFakeCells[0]},
        {"c1", gFakeCells[1]},         {"c2", gFakeCells[2]},        {"sprite", gFakeSprite},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    return n;
}

void tpw_rb_removebridge(uint64_t cell, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    MapDelta a{ax, ay, 0};
    MapDelta b{bx, by, 0};
    RemoveBridge(reinterpret_cast<CTrackCell*>(cell), a, b);
}

// The unit space's width (the port's widescreen); 0 when the library is built without the port fixes.
int32_t tpw_rb_unitwidth(int32_t width) {
    (void)width;
    return 0;
}

uint64_t tpw_rb_peep() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCC_SystemGetPointedPeepThing())); }

}  // extern "C"
