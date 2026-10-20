// Test library for the ride commands on the bump world (unit v51036; libtpw_unit_BMP_SetRideDuration.dylib, tests/test_BMP_SetRideDuration.py), main app:
//   src/BMP_FixRide_BMP_CountTrackSections_BMP_SetRideDuration.cpp   the ride commands, the bump and buoy starts, the Hoardings sinit
//
// TRACE, CBalanceTable::Init and __register_global_object are fakes that record what the real ones read. The world list (0x102b56c0)
// and the two hoardings tables are defined here (the program's definitions are in files this library does not compile).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "HoardingsBalance.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files: the world list, with room for the words at +0x20 and +0x24 (the symbol is gColList)
alignas(16) uint8_t gWorldBlock[0x40] __asm__("_gColList");

// The hoardings tables, their members (the file HoardingsBalance.cpp is not compiled here).
CHoardingsBalance gHoardingsBalance434C;
CHoardingsBalance gHoardingsBalance4130;
CHoardingsBalance::~CHoardingsBalance() {}
const BalanceDescription* CHoardingsBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CHoardingsBalance::GetBalanceValuesStart() { return nullptr; }

// ---- the fakes
void TRACE(const char* format, ...) {
    int64_t arg = 0;
    if (std::strstr(format, "%d") != nullptr) {
        va_list list;
        va_start(list, format);
        arg = va_arg(list, int);
        va_end(list);
    }
    ev("TRACE", {arg}, {format});
}

void CBalanceTable::Init() { ev("Init", {lab(this)}); }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_bmpsd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldBlock},
        {"t434C", &gHoardingsBalance434C},
        {"t4130", &gHoardingsBalance4130},
        {"dtors", gHoardingsDtorVector},
        {"node434C", gHoardingsNode434C},
        {"node4130", gHoardingsNode4130},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bmpsd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The word a Ptr32 field holds for the block at addr.
uint32_t tpw_bmpsd_off(uint64_t addr) { return addr == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(addr))); }

int32_t tpw_bmpsd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    return n;
}

int32_t tpw_bmpsd_setdur(int32_t ride, int32_t duration) { return BMP_SetRideDuration(ride, duration); }
void tpw_bmpsd_worn(int32_t ride) { BMP_RideWorn(ride); }
uint32_t tpw_bmpsd_mesh(int32_t ride) { return static_cast<uint32_t>(BMP_GetCameraMesh(ride)); }
uint64_t tpw_bmpsd_bump(int32_t x, int32_t y, int32_t type) { return reinterpret_cast<uintptr_t>(BMP_StartBump(x, y, type)); }
uint64_t tpw_bmpsd_buoy(int32_t x, int32_t y) { return reinterpret_cast<uintptr_t>(BMP_StartBuoy(x, y)); }
uint64_t tpw_bmpsd_addbuoy(uint64_t ride, int32_t x, int32_t y) {
    return reinterpret_cast<uintptr_t>(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(static_cast<uintptr_t>(ride)), x, y));
}
void tpw_bmpsd_sinit() { __sinit_HoardingsBalance_cpp(); }

}  // extern "C"
