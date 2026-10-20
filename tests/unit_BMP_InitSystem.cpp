// Test library for the advisor's bump world (unit v5659; libtpw_unit_BMP_InitSystem.dylib, tests/test_BMP_InitSystem.py), main app:
//   src/host_leaves_BMP_InitSystem.cpp   BMP_InitSystem, BMP_CloseSystem, BMP_FreeAll, BMP_OpenRide, BMP_CloseRide
//
// The callees outside the unit are recording fakes: TRACE, SetBuoySequence, BMP_SetRidePerformance, BMP_DestroyRide, NS_MacDoze::GlobalAlloc
// and GlobalFree, Rand. GlobalAlloc hands out arena blocks stamped with their size at +4; GlobalFree logs that stamp. The world (gColList's
// symbol, 0x3c bytes) and the sine table are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "effect_manager.h"
#include "rsse.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gHostSineTable[512];

// The world is 0x3c bytes, gColList is declared 0x20: the same symbol, a larger object.
struct BigColList {
    tabCOLLIST list;
    uint32_t extra[8];
};
BigColList gBigColList __asm__("_gColList");

namespace {

int64_t RideIndex(const void* ride) {
    const uint32_t* words = reinterpret_cast<const uint32_t*>(&gBigColList);
    const uint8_t* rides = tpw::gArenaBase + words[2];
    return (static_cast<const uint8_t*>(ride) - rides) / 0xd0;
}

}  // namespace

// ---- the fakes
void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {value}, {format});
}

void SetBuoySequence(tagBUMPRIDE* ride) {
    ev("SetBuoySequence", {RideIndex(ride), ride->state});
}

void BMP_SetRidePerformance(int32_t ride, int32_t speed) { ev("Perf", {ride, speed}); }

void BMP_DestroyRide(int32_t ride) { ev("Destroy", {ride}); }

int32_t Rand() {
    ev("rand", {});
    return static_cast<int32_t>(ret("rand"));
}

namespace NS_MacDoze {

InternalHandle* GlobalAlloc(uint32_t flags, uint32_t size) {
    ev("GlobalAlloc", {flags, size});
    if (ret("GlobalAlloc") == 0) return nullptr;
    uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size));
    *reinterpret_cast<uint32_t*>(block + 4) = size;
    return reinterpret_cast<InternalHandle*>(block);
}

InternalHandle* GlobalFree(InternalHandle* block) {
    int64_t stamp = -1;
    if (block != nullptr) stamp = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(block) + 4);
    ev("GlobalFree", {stamp});
    return nullptr;
}

}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_bmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gBigColList},
        {"sine", gHostSineTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_bmp_base() {
    tpw::ArenaAllocZeroed(16);   // reserves the arena on first use
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase));
}

int32_t tpw_bmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    return n;
}

void tpw_bmp_init() { BMP_InitSystem(); }
void tpw_bmp_close_system() { BMP_CloseSystem(); }
void tpw_bmp_free_all() { BMP_FreeAll(); }
void tpw_bmp_open_ride(int32_t ride) { BMP_OpenRide(ride); }
void tpw_bmp_close_ride(int32_t ride) { BMP_CloseRide(ride); }

}  // extern "C"
