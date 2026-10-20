// Test library for COnlinePerson's seven functions (unit v5133; libtpw_unit_COnlinePerson_SetDest.dylib, tests/test_COnlinePerson_SetDest.py), main app:
//   src/COnlinePerson_SetDest.cpp
//
// The CPeep callees, CDelayedScript::SetSpriteScript, ProcessIdle, the COnlineControl constructor and UpdateAvatarPos, operator new, CTRACE and
// wcscmp are recording fakes. Offsets of `this` are logged relative to the person block.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "online.h"

using namespace calllog;

COnlineControl* gOnlineControl;

namespace {

uint8_t* gPerson = nullptr;

int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p) - gPerson); }
int64_t Pos(MapPos m) { return (static_cast<int64_t>(m.mCell) << 8) | m.mOffset; }

}  // namespace

int32_t wcscmp(const char16_t* a, const char16_t* b) {
    uint32_t ha = 0, hb = 0;
    int64_t la = 0, lb = 0;
    for (const char16_t* p = a; *p; ++p) { ha = ha * 31 + *p; ++la; }
    for (const char16_t* p = b; *p; ++p) { hb = hb * 31 + *p; ++lb; }
    ev("wcscmp", {la, ha, lb, hb});
    while (*a && *a == *b) { ++a; ++b; }
    return static_cast<int32_t>(*a) - static_cast<int32_t>(*b);
}

void CTRACE(uint32_t level, const char* format, ...) {
    uint32_t h = 0;
    int64_t n = 0;
    for (const char* p = format; *p; ++p) { h = h * 31 + static_cast<uint8_t>(*p); ++n; }
    ev("CTRACE", {static_cast<int64_t>(level), n, h});
}

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDestCell", {Off(this), cell.mIndex});
    return static_cast<int32_t>(ret("SetDestCell"));
}
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDestPos", {Off(this), Pos(x), Pos(y)});
    return static_cast<int32_t>(ret("SetDestPos"));
}
void CPeep::Teleport(MapPos x, MapPos y) { ev("Teleport", {Off(this), Pos(x), Pos(y)}); }
void CPeep::UpdateTurn() { ev("UpdateTurn", {Off(this)}); }
void CPeep::SpawnParticleEffect(int32_t effect) { ev("Particle", {Off(this), effect}); }
int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {Off(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("Script", {Off(this), static_cast<int64_t>(script)}); }
void COnlinePerson::ProcessIdle() { ev("Idle", {Off(this)}); }
COnlineControl::COnlineControl() { ev("ctor", {newlab(this)}); }
void COnlineControl::UpdateAvatarPos(MapPos x, MapPos y) { ev("UpdateAvatarPos", {lab(this), Pos(x), Pos(y)}); }

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace online_callee

extern "C" {

uint64_t tpw_op_addr(const char* name) {
    if (std::strcmp(name, "gOnlineControl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineControl));
    return 0;
}

uint64_t tpw_op_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_op_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    return n;
}

namespace {
COnlinePerson* Person(uint64_t p) {
    gPerson = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(p));
    return reinterpret_cast<COnlinePerson*>(gPerson);
}
MapPos Mp(uint32_t cell, uint32_t offset) {
    MapPos m;
    m.mCell = static_cast<uint8_t>(cell);
    m.mOffset = static_cast<uint8_t>(offset);
    return m;
}
}  // namespace

void tpw_op_update_turn(uint64_t p) { Person(p)->UpdateTurn(); }
void tpw_op_model_state(uint64_t p) { Person(p)->ModelState(); }
void tpw_op_walk(uint64_t p) { Person(p)->ProcessWalkingToDest(); }
uint32_t tpw_op_is_my_name(uint64_t p, uint64_t data) {
    std::u16string s(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(data)));
    return Person(p)->IsThisMyName(s) ? 1u : 0u;
}
uint32_t tpw_op_setdest_cell(uint64_t p, uint32_t index) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
    cell.mIndex = static_cast<uint16_t>(index);
    return static_cast<uint32_t>(Person(p)->SetDest(cell));
}
uint32_t tpw_op_setdest_pos(uint64_t p, uint32_t c1, uint32_t o1, uint32_t c2, uint32_t o2) {
    return static_cast<uint32_t>(Person(p)->SetDest(Mp(c1, o1), Mp(c2, o2)));
}
void tpw_op_teleport(uint64_t p, uint32_t c1, uint32_t o1, uint32_t c2, uint32_t o2) { Person(p)->Teleport(Mp(c1, o1), Mp(c2, o2)); }

}  // extern "C"
