// Test library for COnlineControl's command senders and avatar-table accessors (unit v5121; libtpw_unit_COnlineControl_EarmuffsOff.dylib,
// tests/test_COnlineControl_EarmuffsOff.py), main app:
//   src/COnlineControl_..._EarmuffsOff.cpp   VoteForThisPark, EarmuffsOn, EarmuffsOff, GetPerson, SetDestination x2, Teleport x2
//
// The callees outside the unit are recording fakes: the string and vector helpers, DoCommand, the world's used-list walk, GAME_ASSERTMSG and
// COnlinePerson's members. The globals of other files (gGameWorld, gThingArray) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "online.h"

using namespace calllog;

bool OnlineControlTeleportThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y);

// ---- globals of other files
CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
uintptr_t gLastS = 0;        // the address the last string constructor fake saw
const void* gThis = nullptr;   // the expected object and name of the call
const void* gName = nullptr;

uint32_t ThingId(const void* thing) {
    uint16_t id;
    std::memcpy(&id, thing, 2);
    return id;
}
}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
namespace online_callee {
void StringCtor(void* self) {
    gLastS = reinterpret_cast<uintptr_t>(self);
    ev("StringCtor", {0});
}
void WStringCtor(void* self) { ev("WStringCtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gLastS)}); }
void ArgumentVectorPushBack(void* vec, const void* argument) {
    const uint32_t* v = static_cast<const uint32_t*>(vec);
    const uint32_t* a = static_cast<const uint32_t*>(argument);
    ev("PushBack", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(argument) - gLastS), a[2], v[0], v[1], v[2]});
}
void WStringDtor(void* self, int16_t flag) {
    ev("WStringDtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gLastS), flag});
}
void StringDtor(void* self, int16_t flag) {
    ev("StringDtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gLastS), flag});
}
void ArgumentVectorTearDown(void* vec) {
    const uint32_t* v = static_cast<const uint32_t*>(vec);
    ev("TearDown", {v[0], v[1], v[2]});
}
}  // namespace online_callee

void COnlineControl::DoCommand(void* arguments) {
    const uint32_t* v = static_cast<const uint32_t*>(arguments);
    ev("DoCommand", {this == gThis, v[0], v[1], v[2]});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {this == gGameWorld});
    return {static_cast<uint16_t>(ret("head"))};
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {this == gGameWorld, ThingId(thing)});
    return {static_cast<uint16_t>(ret("next"))};
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

bool COnlinePerson::IsThisMyName(std::u16string& name) const {
    ev("IsThisMyName", {ThingId(this), &name == gName});
    return (ret("mine") & 0xff) != 0;
}
int32_t COnlinePerson::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDestMap", {ThingId(this), cell.mIndex});
    return static_cast<int32_t>(ret("setdest"));
}
int32_t COnlinePerson::SetDest(MapPos x, MapPos y) {
    ev("SetDestXY", {ThingId(this), x.mCell, x.mOffset, y.mCell, y.mOffset});
    return static_cast<int32_t>(ret("setdest"));
}
void COnlinePerson::Teleport(MapPos x, MapPos y) {
    ev("PersonTeleport", {ThingId(this), x.mCell, x.mOffset, y.mCell, y.mOffset});
}

extern "C" {

uint64_t tpw_v5121_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"arr", gThingArray},
        {"nul", &gNullThingId},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5121_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5121_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mThingId));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    return n;
}

// ---- the functions
namespace {
COnlineControl* Self(uint64_t p) {
    gThis = reinterpret_cast<const void*>(static_cast<uintptr_t>(p));
    return reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> Thing(uint32_t t) { return {static_cast<uint16_t>(t)}; }
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> Cell(uint32_t c) { return {static_cast<uint16_t>(c)}; }
}  // namespace

void tpw_v5121_vote(uint64_t self) { Self(self)->VoteForThisPark(); }
void tpw_v5121_on(uint64_t self) { Self(self)->EarmuffsOn(); }
void tpw_v5121_off(uint64_t self) { Self(self)->EarmuffsOff(); }
uint64_t tpw_v5121_get(uint64_t self, uint64_t name) {
    gName = reinterpret_cast<const void*>(static_cast<uintptr_t>(name));
    COnlinePerson* p = Self(self)->GetPerson(*reinterpret_cast<std::u16string*>(static_cast<uintptr_t>(name)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}
uint32_t tpw_v5121_setdest_map(uint64_t self, uint32_t thing, uint32_t cell) { return Self(self)->SetDestination(Thing(thing), Cell(cell)); }
uint32_t tpw_v5121_setdest_xy(uint64_t self, uint32_t thing, uint32_t xc, uint32_t xo, uint32_t yc, uint32_t yo) {
    MapPos x{static_cast<uint8_t>(xc), static_cast<uint8_t>(xo)};
    MapPos y{static_cast<uint8_t>(yc), static_cast<uint8_t>(yo)};
    return Self(self)->SetDestination(Thing(thing), x, y);
}
void tpw_v5121_tele_map(uint64_t self, uint32_t thing, uint32_t cell) { Self(self)->Teleport(Thing(thing), Cell(cell)); }
uint32_t tpw_v5121_tele_xy(uint64_t self, uint32_t thing, uint32_t xc, uint32_t xo, uint32_t yc, uint32_t yo) {
    MapPos x{static_cast<uint8_t>(xc), static_cast<uint8_t>(xo)};
    MapPos y{static_cast<uint8_t>(yc), static_cast<uint8_t>(yo)};
    Self(self);
    return OnlineControlTeleportThing(Thing(thing), x, y);
}

}  // extern "C"
