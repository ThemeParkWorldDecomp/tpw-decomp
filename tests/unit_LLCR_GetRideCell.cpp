// Test library for the small accessors, static initialisers and the track animation walk of unit v51063 (libtpw_unit_LLCR_GetRideCell.dylib,
// tests/test_LLCR_GetRideCell.py), main app:
//   src/game/HLSprite.cpp, src/game/park/LLCellMouse.cpp, src/game/park/LLCellRide.cpp, src/game/rides/FlyingRide.cpp, src/game/ui/intrface.cpp
//
// The callees of LLCC_SetTrackAnim and the Destroy functions are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "TrackCell.h"
#include "CellBase.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "rsse.h"
#include "HLSprite.h"
#include "ESP_sprites.h"
#include "analyser.h"
#include "intrface.h"
#include "World.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gLLCMIllegalPlacement;
int32_t gLLCCDB6C;
CObject* gLLCMCurrentObject;
uint16_t gLLCCDB58;
uint16_t gLLCCDB5A;
uint16_t gLLCCDB5C;
uint16_t gLLCCDB68;
uint16_t gLLCCE092;
int32_t gLLCCE094;
const uint8_t* gLLCRRideShape;   // host only: the shape the cells are read from (unit v5L28)
CIntrface gIntrface;
uint8_t gESPDrawEnabled;
ESPSpriteGroup gESPGroups[kESPGroups];
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
void* gFlyBases[100];
tag_sRideAnimSet* gRideAnimSets[16];
CWorld* gGameWorld;

namespace {

int64_t Rel(const void* p) {
    return static_cast<int64_t>(static_cast<int32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gGameWorld)));
}

}  // namespace

// ---- the fakes
uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetStart", {static_cast<int64_t>(*x), static_cast<int64_t>(*y), static_cast<int64_t>(rotation)});
    *x = static_cast<int32_t>(ret("OffsetX"));
    *y = static_cast<int32_t>(ret("OffsetY"));
    return static_cast<uint8_t>(ret("OffsetDir"));
}

const CMapCell* CCellBase::GetMapCellEquiv() const {
    ev("MapCellEquiv", {Rel(this)});
    return nullptr;
}

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("MapCellEquiv", {Rel(this)});
    return nullptr;
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Rel(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {Rel(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

int32_t CTrackCell::IsTrackStartBlock() const {
    ev("IsTrackStartBlock", {Rel(this)});
    return static_cast<int32_t>(ret("IsTrackStartBlock"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Rel(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("NeighbouringCell", {Rel(this), static_cast<int64_t>(direction), static_cast<int64_t>(offset)});
    int64_t v = ret("NeighbouringCell");
    if (v == 0) return nullptr;
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + (v - 1) * 0x28);
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {static_cast<int64_t>(direction)});
    switch (direction) {
        case 1: return 0x10;
        case 0x10: return 1;
        case 4: return 0x40;
        case 0x40: return 4;
        default: return 0;
    }
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(set))), static_cast<int64_t>(a),
                          static_cast<int64_t>(anim), static_cast<int64_t>(loop), static_cast<int64_t>(fbits(speed)), static_cast<int64_t>(b)});
}

void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {static_cast<int64_t>(instance)}); }

extern "C" {

uint64_t tpw_llcr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"illegal", &gLLCMIllegalPlacement}, {"curobj", &gLLCCDB6C}, {"ce092", &gLLCCE092}, {"ce094", &gLLCCE094}, {"shape", &gLLCRRideShape},
        {"db58", &gLLCCDB58}, {"db5a", &gLLCCDB5A}, {"db5c", &gLLCCDB5C}, {"db68", &gLLCCDB68},
        {"db74", &gLLCCDB74}, {"db78", &gLLCCDB78}, {"dc1d8", &gIntrfaceDC1D8}, {"dc1dc", &gIntrfaceDC1DC},
        {"intrface", &gIntrface}, {"espdraw", &gESPDrawEnabled}, {"groups", gESPGroups}, {"sprites", gESPSprites},
        {"flybases", gFlyBases}, {"anims", gRideAnimSets}, {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llcr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 form of an arena address (an offset from the arena base).
uint32_t tpw_llcr_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_llcr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ESPSpriteGroup));
    out[n++] = static_cast<int64_t>(offsetof(ESPSpriteGroup, mFirst));
    out[n++] = static_cast<int64_t>(offsetof(ESPSpriteGroup, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ESP_CSprite, mCustomCount));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mHasSprite));
    out[n++] = static_cast<int64_t>(sizeof(tagSCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mShowFPS));
    return n;
}

// ---- the functions: every argument a uint64, every answer a uint32
uint32_t tpw_llcr_LLCR_GetRideCell(uint64_t col, uint64_t row) { return static_cast<uint32_t>(LLCR_GetRideCell(static_cast<int32_t>(col), static_cast<int32_t>(row))); }
uint32_t tpw_llcr_LLCR_GetRideCellDirection(uint64_t col, uint64_t row) { return LLCR_GetRideCellDirection(static_cast<int32_t>(col), static_cast<int32_t>(row)); }
uint32_t tpw_llcr_LLCR_SetTopLeft(uint64_t word) {
    LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(word >> 16)});
    return 0;
}
uint32_t tpw_llcr_LLCR_GetWidthHeight(uint64_t w, uint64_t h) {
    LLCR_GetWidthHeight(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(w)), reinterpret_cast<int32_t*>(static_cast<uintptr_t>(h)));
    return 0;
}
uint32_t tpw_llcr_LLCC_SetTrackAnim(uint64_t object, uint64_t flag) {
    LLCC_SetTrackAnim(reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)), flag != 0);
    return 0;
}
uint32_t tpw_llcr___sinit_LLCellMouse_cpp() { __sinit_LLCellMouse_cpp(); return 0; }
uint32_t tpw_llcr_LLCM_GetIllegalPlacement() { return static_cast<uint32_t>(LLCM_GetIllegalPlacement()); }
uint32_t tpw_llcr_LLCM_GetCurrentObject() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(LLCM_GetCurrentObject())); }
uint32_t tpw_llcr_LLCCM_DestroyHoardingMesh(uint64_t m) { LLCCM_DestroyHoardingMesh(static_cast<uint32_t>(m)); return 0; }
uint32_t tpw_llcr_LLCCM_DestroyWTrackMesh(uint64_t m) { LLCCM_DestroyWTrackMesh(static_cast<uint32_t>(m)); return 0; }
uint32_t tpw_llcr_LLCCM_DestroyKTrackMesh(uint64_t m) { LLCCM_DestroyKTrackMesh(static_cast<uint32_t>(m)); return 0; }
uint32_t tpw_llcr_LLCCM_DestroyQMesh(uint64_t m) { LLCCM_DestroyQMesh(static_cast<uint32_t>(m)); return 0; }
uint32_t tpw_llcr___sinit_intrface_cpp() { __sinit_intrface_cpp(); return 0; }
uint32_t tpw_llcr_ESP_GetNumberOfCustomAnims(uint64_t set, uint64_t index) {
    return static_cast<uint32_t>(ESP_GetNumberOfCustomAnims(static_cast<ESP_Sprites>(static_cast<int32_t>(set)), static_cast<int32_t>(index)));
}
uint32_t tpw_llcr_ESP_GetNumberOfSprites(uint64_t set) { return static_cast<uint32_t>(ESP_GetNumberOfSprites(static_cast<ESP_Sprites>(static_cast<int32_t>(set)))); }
uint32_t tpw_llcr_DrawHLSprite(uint64_t script) { DrawHLSprite(reinterpret_cast<tagSCRIPT*>(static_cast<uintptr_t>(script))); return 0; }
uint32_t tpw_llcr_ESP_DisableSpriteDraw() { ESP_DisableSpriteDraw(); return 0; }
uint32_t tpw_llcr_ESP_EnableSpriteDraw() { ESP_EnableSpriteDraw(); return 0; }
uint32_t tpw_llcr_FLY_SetMotionScale(uint64_t fly, uint64_t a) { FLY_SetMotionScale(static_cast<int32_t>(fly), static_cast<int32_t>(a)); return 0; }
uint32_t tpw_llcr_FLY_SetCarSize(uint64_t fly, uint64_t a) { FLY_SetCarSize(static_cast<int32_t>(fly), static_cast<int32_t>(a)); return 0; }

}  // extern "C"
