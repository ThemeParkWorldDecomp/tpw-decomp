// Test library for CTrainingCentre::Serialise and UpdateMonthly (libtpw_unit_CTrainingCentre_Serialise.dylib,
// tests/test_CTrainingCentre_Serialise.py), main app:
//   src/game/buildings/TrainingCentre.cpp   the two functions and the three training globals
//
// The callees outside the unit (SAVEVAR/LOADVAR, GAME_ASSERTMSG, the world's used-thing walk, CStaff::TrainMe) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "Debug.h"
#include "Serialise.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
uintptr_t gSelf = 0;

int64_t StrKey(const char* s) {
    int64_t sum = 0;
    int64_t n = 0;
    for (; s[n]; ++n) sum += static_cast<uint8_t>(s[n]);
    return n * 100000 + sum;
}

int64_t Slot(void* value) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gSelf); }
}  // namespace

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {static_cast<int64_t>(condition & 0xff), StrKey(format)});
}

template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    ev("save_i", {static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)), Slot(value), StrKey(name), noLog});
    return static_cast<int32_t>(ret("io"));
}
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    ev("save_b", {static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)), Slot(value), StrKey(name), noLog});
    return static_cast<int32_t>(ret("io"));
}
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    ev("load_i", {static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)), Slot(value), StrKey(name), noLog});
    return static_cast<int32_t>(ret("io"));
}
template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    ev("load_b", {static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)), Slot(value), StrKey(name), noLog});
    return static_cast<int32_t>(ret("io"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("head", {lab(const_cast<CWorld*>(this))});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("head"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("next", {lab(const_cast<CWorld*>(this)), static_cast<int64_t>(*reinterpret_cast<const uint16_t*>(thing))});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("next"));
    return r;
}

void CStaff::TrainMe(int32_t amount) {
    ev("TrainMe", {static_cast<int64_t>(*reinterpret_cast<const uint16_t*>(this)), amount});
}

extern "C" {

uint64_t tpw_ct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"counts", gTrainingCounts}, {"end1", &gTrainingEnd1}, {"end2", &gTrainingEnd2}, {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ct_set_world() { gGameWorld = reinterpret_cast<CWorld*>(&gGameWorld); }

void tpw_ct_set_thing(uint32_t index, uint64_t thing) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }

int32_t tpw_ct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrainingCentre, mF14));
    out[n++] = static_cast<int64_t>(sizeof(CTrainingCentre));
    return n;
}

int32_t tpw_ct_serialise(uint64_t self, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gSelf = static_cast<uintptr_t>(self);
    return reinterpret_cast<CTrainingCentre*>(gSelf)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

void tpw_ct_update(uint64_t self) {
    gSelf = static_cast<uintptr_t>(self);
    reinterpret_cast<CTrainingCentre*>(gSelf)->UpdateMonthly();
}

}  // extern "C"
