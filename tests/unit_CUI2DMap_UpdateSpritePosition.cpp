// Test library for the map panel's sprite position, coaster tracks and grid details (unit v5200; libtpw_unit_CUI2DMap_UpdateSpritePosition.dylib,
// tests/test_CUI2DMap_UpdateSpritePosition.py), main app:
//   src/game/ui/ui_2dmap.cpp   CUI2DMap::UpdateSpritePosition, DrawCoasterTracks,
//   StoreMapGridDetails, and the globals gUI2DMapGridThingListEnd and gUI2DMapColourTable.
//
// The callees outside the unit are fakes that record what the real ones read: the sprite's SetPosition and SetScale, the CDraw object, the coaster
// track queries, the colour scale, the four Antialiased line routines, the cell queries, the assertion and GetDBInfo. The globals of other files
// the unit reads (the screen size, the thing table, the world pointer) are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "platform/d3d_types.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
int32_t gScreenWidth;
int32_t gScreenHeight;
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
TbRect gClipRect;   // what the CDraw fake's GetClipRect answers

// The CDraw members (two TbRenderAreas) are built by the fake constructor; the real TbRenderArea constructor (0x10016630) is outside the unit.
TbRenderArea::TbRenderArea() : TbRenderArea(kCleared) {}

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &b, sizeof(value));
    return value;
}

const void* gDraw = nullptr;               // the CDraw the fake constructor saw last
uint64_t gTrackHead[8];                    // per track: the start control and the track's first control (the "out" block)
uint64_t gTrackOut[8];
uint64_t gCtl[64];                         // the controls' addresses by number
void* gMapObject = nullptr;
void* gSpriteObject = nullptr;
void* gWorldObject = nullptr;

int64_t CellIndex(const void* cell) {
    return (reinterpret_cast<const char*>(cell) - reinterpret_cast<const char*>(gGameWorld) - 0x2d8) / 0x44;
}

int64_t TrackIndex(const void* cell) {
    return (reinterpret_cast<const char*>(cell) - reinterpret_cast<const char*>(gGameWorld) - 0x1102d8) / 0x28;
}

int64_t CtlIndex(const void* control) {
    for (int i = 0; i < 64; i++)
        if (gCtl[i] == reinterpret_cast<uint64_t>(control) && gCtl[i] != 0) return i;
    return -1;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
namespace bfl {
namespace draw {
CDraw::CDraw() {
    gDraw = this;
    ev("CDraw", {0});
}
CDraw::~CDraw() { ev("~CDraw", {this == gDraw, -1}); }
void CDraw::SetCurrent(TbRenderArea* area) { ev("SetCurrent", {this == gDraw, lab(area)}); }
const TbRect& CDraw::GetClipRect() const {
    ev("GetClipRect", {this == gDraw});
    return gClipRect;
}
}  // namespace draw
}  // namespace bfl

void CUI2DMapSprite::SetPosition(const _D3DVECTOR& position) {
    ev("SetPosition", {lab(this), CanonBits(position.x), CanonBits(position.y), CanonBits(position.z)});
}

void CUI2DMapSprite::SetScale(float scale) { ev("SetScale", {lab(this), CanonBits(scale)}); }

int32_t ui_GetNumCoasterTracks() {
    ev("ui_GetNumCoasterTracks", {0});
    return static_cast<int32_t>(ret("ui_GetNumCoasterTracks"));
}

tag_sBezierControl* GetNthCoasterTrackStart(uint32_t index, void** control) {
    ev("GetNthCoasterTrackStart", {index});
    *control = reinterpret_cast<void*>(gTrackOut[index & 7]);
    return reinterpret_cast<tag_sBezierControl*>(gTrackHead[index & 7]);
}

bfl::draw::CColour UI2DMReturnScaleColour(int32_t index, const bfl::draw::CColour& a, const bfl::draw::CColour& b, const bfl::draw::CColour& c) {
    const char* table = reinterpret_cast<const char*>(gUI2DMapColourTable);
    ev("UI2DMReturnScaleColour", {index, reinterpret_cast<const char*>(&a) - table, reinterpret_cast<const char*>(&b) - table,
                                  reinterpret_cast<const char*>(&c) - table});
    bfl::draw::CColour result;
    const uint32_t word0 = static_cast<uint32_t>(ret("scw"));
    result.mRed = static_cast<uint8_t>(word0 >> 24);
    result.mGreen = static_cast<uint8_t>(word0 >> 16);
    result.mBlue = static_cast<uint8_t>(word0 >> 8);
    result.mAlpha = static_cast<uint8_t>(word0);
    for (int i = 0; i < 8; i++) result.mFormatCache[i] = static_cast<uint32_t>(ret("scw"));
    return result;
}

void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point) {
    ev("BezierPosition", {CtlIndex(a), CtlIndex(b), fbits(t)});
    point->x = FromBits(ret("bx"));
    point->y = FromBits(ret("by"));
    point->z = FromBits(ret("bz"));
}

namespace tpw_antialias {
template <int N>
void Antialiased(const tpw_intensify::CTraits<N>&, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour, const TbPoint& from,
                 const TbPoint& to) {
    const uint32_t word0 = (static_cast<uint32_t>(colour.mRed) << 24) | (static_cast<uint32_t>(colour.mGreen) << 16) |
                           (static_cast<uint32_t>(colour.mBlue) << 8) | colour.mAlpha;
    ev("AAColour", {word0, colour.mFormatCache[0], colour.mFormatCache[1], colour.mFormatCache[2], colour.mFormatCache[3], colour.mFormatCache[4],
                    colour.mFormatCache[5], colour.mFormatCache[6], colour.mFormatCache[7]});
    ev("Antialiased", {N, lab(&area), clip.left, clip.top, clip.right, clip.bottom, from.x, from.y, to.x, to.y, &clip == &gClipRect});
}
template void Antialiased<1>(const tpw_intensify::CTraits<1>&, const TbRenderArea&, const TbRect&, const bfl::draw::CColour&, const TbPoint&, const TbPoint&);
template void Antialiased<2>(const tpw_intensify::CTraits<2>&, const TbRenderArea&, const TbRect&, const bfl::draw::CColour&, const TbPoint&, const TbPoint&);
template void Antialiased<3>(const tpw_intensify::CTraits<3>&, const TbRenderArea&, const TbRect&, const bfl::draw::CColour&, const TbPoint&, const TbPoint&);
template void Antialiased<4>(const tpw_intensify::CTraits<4>&, const TbRenderArea&, const TbRect&, const bfl::draw::CColour&, const TbPoint&, const TbPoint&);
}  // namespace tpw_antialias

int32_t CMapCell::IsHoarding() const {
    ev("IsHoarding", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsHoarding"));
}
int32_t CMapCell::IsRide() const {
    ev("IsRide", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsRide"));
}
int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsEntrance"));
}
int32_t CMapCell::IsExit() const {
    ev("IsExit", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsExit"));
}
int32_t CMapCell::IsTrack() const {
    ev("IsTrack", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}
int32_t CMapCell::IsCoasterPylon() const {
    ev("IsCoasterPylon", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsCoasterPylon"));
}
int32_t CMapCell::IsPath() const {
    ev("IsPath", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsPath"));
}
int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsQueue"));
}
int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {CellIndex(this), flag});
    return static_cast<int32_t>(ret("IsFlag"));
}

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {CellIndex(this)});
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + ret("equiv") * 0x28);
}

int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {TrackIndex(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The thing's own id sits in a halfword at +0x30 of the test's thing blocks; the ride record is the block's +0x40 (its +0x7a8 word is the kind).
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {*reinterpret_cast<const uint16_t*>(reinterpret_cast<const char*>(this) + 0x30)});
    return reinterpret_cast<RideInfo*>(const_cast<char*>(reinterpret_cast<const char*>(this)) + 0x40);
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", gThingArray},
        {"gworld", &gGameWorld},
        {"end", &gUI2DMapGridThingListEnd},
        {"colours", gUI2DMapColourTable},
        {"sw", &gScreenWidth},
        {"sh", &gScreenHeight},
        {"rect", &gClipRect},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The persistent objects (made on first use): 0 the map, 1 the sprite, 2 the world.
uint64_t tpw_cm_obj(int32_t which) {
    if (gMapObject == nullptr) {
        gMapObject = tpw::ArenaAllocZeroed(0x20200);
        gSpriteObject = tpw::ArenaAllocZeroed(0x40);
        gWorldObject = tpw::ArenaAllocZeroed(0x1da748 + 0x100);
    }
    void* object = which == 0 ? gMapObject : which == 1 ? gSpriteObject : gWorldObject;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

void tpw_cm_settrack(int32_t index, uint64_t head, uint64_t out) {
    gTrackHead[index & 7] = head;
    gTrackOut[index & 7] = out;
}

void tpw_cm_setctl(int32_t index, uint64_t address) { gCtl[index & 63] = address; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = static_cast<int64_t>(sizeof(tag_sBezierControl));
    out[n++] = static_cast<int64_t>(offsetof(tag_sBezierControl, mNext));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mX));
    return n;
}

// ---- the functions, one wrapper each with C types only
void tpw_cm_update(uint64_t map, uint64_t sprite) {
    reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->UpdateSpritePosition(reinterpret_cast<CUI2DMapSprite*>(static_cast<uintptr_t>(sprite)));
}
void tpw_cm_draw(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->DrawCoasterTracks(); }
void tpw_cm_store(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->StoreMapGridDetails(); }

}  // extern "C"
