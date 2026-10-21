// Test library for the ride track counting, performance and teardown (unit v5344; libtpw_unit_BMP_CountTrackSections.dylib,
// tests/test_BMP_CountTrackSections.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_CountTrackSections, BMP_FreeTrack, BMP_SetRidePerformance, BMP_DestroyRide;
//                                                and the globals gRidePerfMax, gRidePerfMin
//
// The callees outside the unit (BMP_DOCK, BMP_CloseRide, TRACE, FreeBuoysForRide, FreeTrackForRide, FreeColObjectsForRide) are fakes
// that record what the real ones read; pointers are logged as the index of the slot or record in its table (-1 for NULL).
// gColList, defined by another file, is defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPAdvisor.h"
#include "rsse.h"

using namespace calllog;

tabCOLLIST gColList;

namespace {

uint32_t WorldWord(uint32_t offset) { return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(&gColList) + offset); }

int64_t Index(const void* p, uint32_t tableOffset, int64_t size) {
    if (p == nullptr) return -1;
    return (static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase) + WorldWord(tableOffset))) / size;
}

}  // namespace

int32_t BMP_DOCK(tagBUMP* bump) {
    ev("DOCK", {Index(bump, 0x10, 0xac)});
    return 0;
}

int32_t BMP_CloseRide(int32_t ride) {
    ev("CloseRide", {ride});
    return 0;
}

void FreeBuoysForRide(tagBUMPRIDE* ride, int32_t n) { ev("FreeBuoys", {Index(ride, 0x8, 0xd0), n}); }

void FreeColObjectsForRide(tagBUMPRIDE* ride) { ev("FreeColObjects", {Index(ride, 0x8, 0xd0)}); }

void FreeTrackForRide(tagBUMPRIDE* ride) { ev("FreeTrack", {Index(ride, 0x8, 0xd0)}); }

void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {value}, {format});
}

extern "C" {

uint64_t tpw_bmpct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gColList},
        {"pmax", &gRidePerfMax},
        {"pmin", &gRidePerfMin},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bmpct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_bmpct_base() {
    if (tpw::gArenaBase == nullptr) tpw::ArenaAllocZeroed(16);  // the arena may start on first use
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_bmpct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLLIST));
    return n;
}

int32_t tpw_bmpct_count(int32_t ride, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    return BMP_CountTrackSections(ride, reinterpret_cast<int32_t*>(a), reinterpret_cast<int32_t*>(b), reinterpret_cast<int32_t*>(c),
                                  reinterpret_cast<int32_t*>(d));
}

void tpw_bmpct_free(int32_t ride) { BMP_FreeTrack(ride); }

void tpw_bmpct_perf(int32_t ride, int32_t speed) { BMP_SetRidePerformance(ride, speed); }

void tpw_bmpct_destroy(int32_t ride) { BMP_DestroyRide(ride); }

}  // extern "C"
