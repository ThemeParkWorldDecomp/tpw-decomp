// Test library for the ride creation (unit v5558; libtpw_unit_BMP_CreateRide.dylib, tests/test_BMP_CreateRide.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_CreateRide
//
// The callees outside the unit (BMP_StartColObject, BMP_AddBuoyToRide, BMP_StartColObjectForRide, TRACE) are fakes that record the
// arguments the real ones read; the two that answer an object answer the block the test names. The globals of other files the unit
// reads (the list, the templates, the performance clamps, the sine table) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tabCOLLIST gColList;
int32_t gRidePerfMax = 100;
int32_t gRidePerfMin = 0;
uint32_t gRideTemplates[14 * 0x34];
int32_t gHostSineTable[512];
static uint64_t gColRet;     // the answer of BMP_StartColObject (a full host address)
static uint64_t gBuoyRet;    // the answer of BMP_AddBuoyToRide

namespace {

// The index of a ride record in the list, the way the PowerPC stub logs it.
int64_t RideIndex(tagBUMPRIDE* ride) {
    uint32_t rides = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<tagBUMPWORLD*>(&gColList)->rides.get()));
    return static_cast<int64_t>((static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ride)) - rides) / 0xd0u);
}

}  // namespace

// ---- the fakes
tabCOLOBJECT* BMP_StartColObject(int32_t x, int32_t y, int32_t radius) {
    ev("BMP_StartColObject", {x, y, radius});
    return reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(gColRet));
}

tabCOLOBJECT* BMP_AddBuoyToRide(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    ev("BMP_AddBuoyToRide", {RideIndex(ride), x, y});
    return reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(gBuoyRet));
}

void BMP_StartColObjectForRide(tagBUMPRIDE* ride, int32_t x, int32_t y, int32_t shape) {
    ev("BMP_StartColObjectForRide", {RideIndex(ride), x, y, shape});
}

void TRACE(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t v = va_arg(ap, int32_t);
    va_end(ap);
    ev("TRACE", {v}, {format});
}

extern "C" {

uint64_t tpw_cr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gColList},
        {"templ", gRideTemplates},
        {"pmax", &gRidePerfMax},
        {"pmin", &gRidePerfMin},
        {"sine", gHostSineTable},
        {"colret", &gColRet},
        {"buoyret", &gBuoyRet},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The arena's first byte: a Ptr32 field holds an address minus this.
uint64_t tpw_cr_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

uint64_t tpw_cr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabCOLLIST));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(sizeof(gRideTemplates));
    return n;
}

uint32_t tpw_cr_create(int32_t kind, int32_t x, int32_t y, int32_t rot, int32_t id, int32_t f10) {
    return static_cast<uint32_t>(BMP_CreateRide(kind, x, y, rot, id, f10));
}

}  // extern "C"
