// Test library for the bump ride's peep lists and BMP_LeaveBump (unit v5139; libtpw_unit_BMP_PollForPeeps.dylib, tests/test_BMP_PollForPeeps.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_PollForPeeps, BMP_AddPeepToRideLeavingList, BMP_AddPeepToRideJoiningList, BMP_LeaveBump
//
// The callees outside the unit (TRACE, RemovePeepsFromBump, FlushDeferredAnim, SetCurRideAnim) are fakes recording their arguments.
// gColList is a 0x100 byte block here (the Mac's list is larger than the header's struct); gRideAnimSets is a table of native pointers.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(16) char gColListStorage[0x100] asm("_gColList");
tag_sRideAnimSet* gRideAnimSets[64];

// ---- the fakes
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

int32_t RemovePeepsFromBump(tagBUMP* bump, int32_t emit) {
    ev("RemovePeepsFromBump", {lab(bump), emit});
    return static_cast<int32_t>(ret("RemovePeepsFromBump"));
}

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) { ev("FlushDeferredAnim", {lab(set), flag}); }

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}

extern "C" {

uint64_t tpw_pfp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", gColListStorage},
        {"sets", gRideAnimSets},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pfp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pfp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, x));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMP));
    return n;
}

uint64_t tpw_pfp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_pfp_poll(int32_t ride) { return BMP_PollForPeeps(ride); }
void tpw_pfp_leaving(int32_t ride, uint32_t peep) { BMP_AddPeepToRideLeavingList(ride, peep); }
int32_t tpw_pfp_joining(int32_t ride, uint32_t peep) { return BMP_AddPeepToRideJoiningList(ride, peep); }
void tpw_pfp_leave(uint64_t bump) { BMP_LeaveBump(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }

}  // extern "C"
