// Test library for the constructors of the things a new level makes (libtpw_unit_ThingCtors.dylib; see tests/test_ThingCtors.py):
// src/game/Thing.cpp, src/game/advisor/Advisor.cpp, src/game/buildings/MechanicHQ.cpp, src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/Weather.cpp, src/game/ui/vector2d.cpp, with the six leaf part constructors inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every callee outside the unit is a fake that records through calllog: CWorld::AddThingToThingList, CThing::GetThingId,
// CMapCell::InsertThing, TRACE, CMessageCentre::Subscribe, CWeather::FindNewQuality, CAdvisor::Init, CTagSystem::Init,
// CRain::GameLoad and the nothrow operator new (the lazy message centre; the case can make it fail). The 29 headers of a new centre
// stand for __construct_array: the first one logs one event, as in tests/unit_NewLevel.cpp.
//
// The object under test lives in gBlock (the Mac's size plus guard bytes); the test loads fill bytes into it before the call and dumps
// it after. The globals gWorld, gWorldMap, gMessageCentre and gei_Rain are set from the case and reported as a label (the test's
// blocks and the centre made during the call) or as the raw low 32 bits.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "tb_rain.h"

using namespace calllog;

namespace {

constexpr std::size_t kBlockSize = 0x16f0 + 0x100;   // the largest object (the advisor) plus guard bytes
alignas(16) unsigned char gBlock[kBlockSize];
alignas(16) unsigned char gWorldBlock[16];           // label 1: a world pointer the case can hand to the code
alignas(16) unsigned char gCentreBlock[0x1d0];       // label 3: an existing message centre
alignas(16) unsigned char gRainBlock[16];            // label 4: a rain object
void* gNewCentre = nullptr;                          // the block the fake operator new made last (its first header logs the event)

// The first twelve bytes of a thing as three numbers by field (the Mac's halfwords are big-endian, the host's native, so the fields are
// compared by value): the id, kind and dead flag; the four cell bytes; the two list links.
struct Fields {
    int64_t w0, w1, w2;
};
Fields Snap(const CThing* thing) {
    return {(static_cast<int64_t>(thing->mThingId) << 16) | (thing->mKind << 8) | thing->mDead,
            (static_cast<int64_t>(thing->mCellX) << 24) | (thing->mCellXSub << 16) | (thing->mCellY << 8) | thing->mCellYSub,
            (static_cast<int64_t>(thing->mCellPrev) << 16) | thing->mCellNext};
}

// A word of an object at `offset` by value.
int64_t Native32(const void* object, int offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const unsigned char*>(object) + offset, 4);
    return value;
}

// A pointer in the shape both sides report: a label, or the raw low 32 bits (with a mark so that it differs from a label).
int64_t Canon(const void* pointer) {
    const int32_t label = lab(pointer);
    if (label >= 0) return label;
    return (1ll << 33) | static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}

}  // namespace

// ---- Globals the code reads and writes (the launcher gets zero definitions) -------------------------------------------------------
CWorld* gWorld;
CMap* gWorldMap;
CMessageCentre* gMessageCentre;
CRain* gei_Rain;

// ---- The fakes -----------------------------------------------------------------------------------------------------------------
// The world's thing table: the world pointer, the thing and its first twelve bytes (three words) at that moment, so that the order of
// the stores shows. The answer is the case's queue "row".
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::AddThingToThingList(CThing* thing) {
    const Fields f = Snap(thing);
    ev("AddThingToThingList", {lab(this), lab(thing), f.w0, f.w1, f.w2});
    return {static_cast<uint16_t>(ret("row", 0))};
}
// Copies the id at +0; the thing's twelve bytes are recorded again (every store of the constructor precedes this call).
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const Fields f = Snap(this);
    ev("GetThingId", {lab(this), mThingId, f.w0, f.w1, f.w2});
    return {mThingId};
}
// The cell as an offset from the map's base (a signed number) and the id.
void CMapCell::InsertThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("InsertThing", {static_cast<int64_t>(reinterpret_cast<intptr_t>(this) - reinterpret_cast<intptr_t>(gWorldMap)), id.mIndex});
}
void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t a = va_arg(args, int32_t);
    const int32_t b = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {a, b}, {format});
}
// The centre, the thing, the type, the thing's first twelve bytes and (for a weather thing, kind 0xf) the word at +0x5c, which shows
// that the first store of 0.0f to the wind direction happens before the first subscription.
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    const Fields f = Snap(&thing);
    const int64_t wind = thing.mKind == 0xf ? Native32(&thing, 0x5c) : 0;
    ev("Subscribe", {lab(this), lab(&thing), type, f.w0, f.w1, f.w2, wind});
    return true;
}
uint32_t CWeather::FindNewQuality() const {
    ev("CWeather::FindNewQuality", {lab(this)});
    return static_cast<uint32_t>(ret("quality", 0));
}
int32_t CAdvisor::Init() {
    ev("CAdvisor::Init", {lab(this)});
    return 1;
}
void CTagSystem::Init() { ev("CTagSystem::Init", {lab(this)}); }
int32_t CRain::GameLoad() {
    ev("CRain::GameLoad", {lab(this)});
    return 1;
}
// operator new with a null answer: the case's queue "newnull" says whether the call fails (the Mac side logs the same event).
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = ::operator new(size);
    return gNewCentre;
}
// The 29 headers of a new centre: the first stands for __construct_array's call (its address labels the centre; the PowerPC side checks
// the constructor and destructor tables and the sizes).
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {

void* tpw_tc_block() { return gBlock; }
int32_t tpw_tc_block_size() { return static_cast<int32_t>(kBlockSize); }
void tpw_tc_load(const uint8_t* bytes, int32_t n) { std::memcpy(gBlock, bytes, static_cast<std::size_t>(n)); }
void tpw_tc_dump(uint8_t* out, int32_t n) { std::memcpy(out, gBlock, static_cast<std::size_t>(n)); }
void* tpw_tc_world_block() { return gWorldBlock; }
void* tpw_tc_centre_block() { return gCentreBlock; }
void* tpw_tc_rain_block() { return gRainBlock; }

// The four globals as raw pointer values (the test hands in the addresses of its blocks, 0, or a random 32 bit word).
void tpw_tc_set_globals(uint64_t world, uint64_t map, uint64_t centre, uint64_t rain) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gei_Rain = reinterpret_cast<CRain*>(static_cast<uintptr_t>(rain));
    gNewCentre = nullptr;
}
void tpw_tc_get_globals(int64_t* out) {
    out[0] = Canon(gWorld);
    out[1] = Canon(gWorldMap);
    out[2] = Canon(gMessageCentre);
    out[3] = Canon(gei_Rain);
}

// op: 0 CThing (a = the map cell id, b = the model), 1 CStaffHQ, 2 CMechanicHQ, 3 CAdvisor, 4 CTagSystem, 5 CResearchLab, 6 CWeather,
// 7 CUIMsgReceiver (the order of the test module's table). The objects are built over the fill bytes: a class with a user provided
// constructor is not zeroed.
void tpw_tc_call(int32_t op, uint32_t a, uint32_t b) {
    switch (op) {
    case 0: new (gBlock) CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(a)}, static_cast<CThing::ThingModel>(b)); break;
    case 1: new (gBlock) CStaffHQ; break;
    case 2: new (gBlock) CMechanicHQ; break;
    case 3: new (gBlock) CAdvisor; break;
    case 4: new (gBlock) CTagSystem; break;
    case 5: new (gBlock) CResearchLab; break;
    case 6: new (gBlock) CWeather; break;
    case 7: new (gBlock) CUIMsgReceiver; break;
    default: break;
    }
}

}  // extern "C"
