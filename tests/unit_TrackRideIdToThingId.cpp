// Test library for TrackRideIdToThingId (unit v5124; libtpw_unit_TrackRideIdToThingId.dylib, tests/test_TrackRideIdToThingId.py), main app:
//   src/game/rides/BumperRide.cpp   TrackRideIdToThingId, gUsedListEnd
//
// The callees outside the unit (CWorld::GetUsedThingHead, CWorld::GetNextUsedThing, CThing::GetThingId, GAME_ASSERTMSG) are recording fakes
// answering from the test's queues. gGameWorld and gThingArray are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

// ---- the fakes
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {this == gGameWorld ? 1 : 0});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("GetUsedThingHead"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {this == gGameWorld ? 1 : 0, thing->mThingId});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("GetNextUsedThing"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {mThingId});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return r;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition & 0xff}, {format});
}

extern "C" {

uint64_t tpw_trti_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameWorld", &gGameWorld},
        {"gThingArray", &gThingArray[0]},
        {"gUsedListEnd", &gUsedListEnd},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_trti_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_trti_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mThingId));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    return n;
}

void tpw_trti_run(uint64_t out, int32_t rideId) { TrackRideIdToThingId(reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(out)), rideId); }

}  // extern "C"
