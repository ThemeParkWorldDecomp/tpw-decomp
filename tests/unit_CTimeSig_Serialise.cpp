// Test library for CTimeSig::Serialise and SecondsSinceLastUpdate (unit v5117; libtpw_unit_CTimeSig_Serialise.dylib,
// tests/test_CTimeSig_Serialise.py), main app:
//   src/game/TPOptions.cpp   the inline members of include/TimeSig.h
//
// SAVEVAR/LOADVAR<MacULong> and CWorldSavvy::GetWorld are fakes recording their arguments; gWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "World.h"
#include "Serialise.h"
#include "WorldSavvy.h"

using namespace calllog;

CWorld* gWorld;

namespace {

int64_t NameHash(const char* name) {
    int64_t sum = 0;
    int64_t n = 0;
    for (; *name; ++name, ++n) sum += static_cast<uint8_t>(*name);
    return sum + 256 * n;
}

}  // namespace

template <>
int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    ev("SAVEVAR", {lab(file), static_cast<int64_t>(*reinterpret_cast<uint32_t*>(value)), NameHash(name), noLog});
    return static_cast<int32_t>(ret("SAVEVAR"));
}

template <>
int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    uint32_t old = *reinterpret_cast<uint32_t*>(value);
    ev("LOADVAR", {lab(file), static_cast<int64_t>(old), NameHash(name), noLog});
    *reinterpret_cast<uint32_t*>(value) = static_cast<uint32_t>(ret("LOADVAL"));
    return static_cast<int32_t>(ret("LOADVAR"));
}

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {});
    return *gWorld;
}

#include "TimeSig.h"

extern "C" {

uint64_t tpw_ts_addr(const char*) { return 0; }

uint64_t tpw_ts_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ts_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTimeSig, mTimeSig));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    return n;
}

void tpw_ts_setworld(uint64_t world) { gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }

int32_t tpw_ts_serialise(uint64_t object, uint64_t file, int32_t save, int32_t version, uint32_t flags) {
    return reinterpret_cast<CTimeSig*>(static_cast<uintptr_t>(object))
        ->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, flags);
}

int32_t tpw_ts_seconds(uint64_t object) {
    return reinterpret_cast<CTimeSig*>(static_cast<uintptr_t>(object))->SecondsSinceLastUpdate();
}

}  // extern "C"
