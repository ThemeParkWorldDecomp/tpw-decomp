// Test library for CThing::Move, FinalDestroy and Serialise (unit v5927; libtpw_unit_CThing_Move_2.dylib, tests/test_CThing_Move_2.py), main app:
//   src/Thing_receive_CThing_Move_2.cpp
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "MapCell.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "World.h"

using namespace calllog;

CMap* gWorldMap;
CThing* gESPF20;
CMessageCentre* gMessageCentre;

namespace {
uintptr_t gCur = 0;        // the thing under test
void* gPreset = nullptr;   // the message centre before the call
void* gNew = nullptr;      // the block operator new made
typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;
}  // namespace

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition & 0xff}, {format});
}

void CMapCell::RemoveThing(ThingIdTag id) {
    ev("RemoveThing", {static_cast<int64_t>(reinterpret_cast<char*>(this) - reinterpret_cast<char*>(gWorldMap)), id.mIndex});
}

void CMapCell::InsertThing(ThingIdTag id) {
    ev("InsertThing", {static_cast<int64_t>(reinterpret_cast<char*>(this) - reinterpret_cast<char*>(gWorldMap)), id.mIndex});
}

void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }

int32_t CMessageCentre::UnsubscribeAll(const CThing& thing) {
    int64_t which = reinterpret_cast<void*>(this) == gPreset ? 1 : (reinterpret_cast<void*>(this) == gNew ? 2 : 0);
    ev("UnsubscribeAll", {which, static_cast<int64_t>(reinterpret_cast<uintptr_t>(&thing) - gCur)});
    return 0;
}

namespace staffhq_callee {
void* OperatorNew(uint32_t size) {
    ev("operator new", {size});
    gNew = tpw::ArenaAllocZeroed(size);
    return gNew;
}
void ConstructSubscriptions(void* mem) { ev("__construct_array", {mem == gNew ? 1 : 0, 0x10, 0x1d}); }
}  // namespace staffhq_callee

// The real templates read and write a host short (the Mac's are big-endian, the host's little-endian); the fakes do the same, so a
// Serialise that hands them the Mac's byte pair unconverted fails here.
template<> int32_t SAVEVAR<int16_t>(void* file, int16_t* value, const char* name, int32_t noLog) {
    ev("SAVEVAR<short>", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gCur), *value, noLog}, {name});
    return static_cast<int32_t>(ret("SAVEVAR"));
}
template<> int32_t LOADVAR<int16_t>(void* file, int16_t* value, const char* name, int32_t noLog) {
    ev("LOADVAR<short>", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gCur), noLog}, {name});
    *value = static_cast<int16_t>(static_cast<uint16_t>(ret("LOADVALUE")));
    return static_cast<int32_t>(ret("LOADVAR"));
}
template<> int32_t SAVEVARSTRUCT<ThingIdTag>(void* file, ThingIdTag* value, const char* name, int32_t noLog) {
    ev("SAVEVARSTRUCT", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gCur), value->mIndex, noLog}, {name});
    return static_cast<int32_t>(ret("SAVEVARSTRUCT"));
}
template<> int32_t LOADVARSTRUCT<ThingIdTag>(void* file, ThingIdTag* value, const char* name, int32_t noLog) {
    ev("LOADVARSTRUCT", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gCur), noLog}, {name});
    value->mIndex = static_cast<uint16_t>(ret("LOADSTRUCTVALUE"));
    return static_cast<int32_t>(ret("LOADVARSTRUCT"));
}

extern "C" {

uint64_t tpw_cm2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorldMap", &gWorldMap},
        {"gESPF20", &gESPF20},
        {"gMessageCentre", &gMessageCentre},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cm2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cm2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellPrev));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellNext));
    out[n++] = static_cast<int64_t>(sizeof(CThing));
    return n;
}

void tpw_cm2_move(uint64_t thing, uint32_t xc, uint32_t xo, uint32_t yc, uint32_t yo) {
    gCur = static_cast<uintptr_t>(thing);
    MapPos x, y;
    x.mCell = static_cast<uint8_t>(xc);
    x.mOffset = static_cast<uint8_t>(xo);
    y.mCell = static_cast<uint8_t>(yc);
    y.mOffset = static_cast<uint8_t>(yo);
    reinterpret_cast<CThing*>(thing)->Move(x, y);
}

void tpw_cm2_final(uint64_t thing) {
    gCur = static_cast<uintptr_t>(thing);
    gPreset = gMessageCentre;
    gNew = nullptr;
    reinterpret_cast<CThing*>(thing)->FinalDestroy();
}

uint32_t tpw_cm2_serialise(uint64_t thing, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gCur = static_cast<uintptr_t>(thing);
    return static_cast<uint32_t>(reinterpret_cast<CThing*>(thing)->Serialise(reinterpret_cast<void*>(file), save, version, unused));
}

}  // extern "C"
