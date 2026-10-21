// Test library for the CPeep functions of Peep.cpp (unit v5281; libtpw_unit_CPeep_PlaySound.dylib, tests/test_CPeep_PlaySound.py), main app:
//   src/game/people/Peep.cpp
//
// The callees outside the unit are fakes that record the arguments (pointers as offsets from the peep) and answer from the test's queues.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
extern uint16_t gPeepNullCellId;

namespace {

uintptr_t gBase;   // the peep of the current call
constexpr uintptr_t kMapBase = 0x10000000;

int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gBase); }
float BitsF(int64_t b) {
    uint32_t u = static_cast<uint32_t>(b);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
int64_t CellIdx(const CMapCell* c) { return (static_cast<int64_t>(reinterpret_cast<uintptr_t>(c)) - static_cast<int64_t>(kMapBase)) / 0x44; }

}  // namespace

// ---- the fakes
void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t*) {
    ev("SCR_GetScriptPosition", {handle});
    *x = BitsF(ret("scrx"));
    *y = BitsF(ret("scry"));
    *z = BitsF(ret("scrz"));
}
int32_t SOUND_SendEvent(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    ev("SOUND_SendEvent", {a, b, c, d, e, f});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}
CMap* CWorldSavvy::GetMap() const {
    ev("GetMap", {Off(this)});
    return reinterpret_cast<CMap*>(kMapBase);
}
int32_t CMapCell::IsPath() const { ev("IsPath", {CellIdx(this)}); return static_cast<int32_t>(ret("IsPath")); }
int32_t CMapCell::IsQueue() const { ev("IsQueue", {CellIdx(this)}); return static_cast<int32_t>(ret("IsQueue")); }
int32_t CMapCell::IsEntrance() const { ev("IsEntrance", {CellIdx(this)}); return static_cast<int32_t>(ret("IsEntrance")); }
int32_t CMapCell::IsExit() const { ev("IsExit", {CellIdx(this)}); return static_cast<int32_t>(ret("IsExit")); }
int32_t CMapCell::IsGrass() const { ev("IsGrass", {CellIdx(this)}); return static_cast<int32_t>(ret("IsGrass")); }
void Pilot::reset(int32_t x, int32_t y, int32_t vx, int32_t vy) { ev("reset", {Off(this), x, y, vx, vy}); }
void Pilot::get_pos(int32_t& x, int32_t& y) {
    ev("get_pos", {Off(this), Off(&x), Off(&y)});
    x = static_cast<int32_t>(ret("gpx"));
    y = static_cast<int32_t>(ret("gpy"));
}
void Pilot::new_target(int32_t x, int32_t y) { ev("new_target", {Off(this), x, y}); }
void Pilot::set_speed(float speed) {
    uint32_t bits = fbits(speed);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    ev("set_speed", {Off(this), static_cast<int64_t>(bits)});
}
void CDelayedScript::ProcessSpriteScripts() { ev("ProcessSpriteScripts", {Off(this)}); }
void CThing::Move(MapPos x, MapPos y) {
    ev("Move", {Off(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
uint32_t CMap::MakeNewTimeStamp() {
    ev("MakeNewTimeStamp", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gWorld))});
    return static_cast<uint32_t>(ret("MakeNewTimeStamp"));
}
bool CPeep::IsLocalAreaUnchangedSinceBecomingStranded() {
    ev("IsLocal", {Off(this)});
    return ret("IsLocal") != 0;
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Off(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    uint32_t a = va_arg(ap, uint32_t);
    uint32_t b = va_arg(ap, uint32_t);
    va_end(ap);
    ev("CTRACE", {level, a, b}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition & 0xff}, {format});
}

extern "C" {

uint64_t tpw_cp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"nullcell", &gPeepNullCellId},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    return n;
}

// MapPos words: the pair is the high half of the PowerPC register word (cell byte high).
static MapPos Pos(uint32_t w) {
    MapPos p;
    p.mCell = static_cast<uint8_t>(w >> 24);
    p.mOffset = static_cast<uint8_t>(w >> 16);
    return p;
}
static CPeep* P(uint64_t peep) {
    gBase = static_cast<uintptr_t>(peep);
    return reinterpret_cast<CPeep*>(gBase);
}

void tpw_cp_play(uint64_t peep, int32_t a, int32_t b) { P(peep)->PlaySound(a, b); }
int32_t tpw_cp_inside(uint64_t peep) { return P(peep)->IsInsidePark(); }
void tpw_cp_teleport(uint64_t peep, uint32_t x, uint32_t y) { P(peep)->Teleport(Pos(x), Pos(y)); }
void tpw_cp_turn(uint64_t peep) { P(peep)->UpdateTurn(); }
uint32_t tpw_cp_isstranded(uint64_t peep) { return P(peep)->IsStranded() ? 1u : 0u; }
void tpw_cp_setstranded(uint64_t peep, int32_t v) { P(peep)->SetStranded(v); }
int32_t tpw_cp_setdest_pos(uint64_t peep, uint32_t x, uint32_t y) { return P(peep)->SetDest(Pos(x), Pos(y)); }
int32_t tpw_cp_setdest_cell(uint64_t peep, uint32_t w) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(w >> 16);
    return P(peep)->SetDest(id);
}

}  // extern "C"
