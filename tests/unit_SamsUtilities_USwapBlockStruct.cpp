// Test library for SamsUtilities::USwapBlockStruct<FLYBOY> (unit v5648; libtpw_unit_SamsUtilities_USwapBlockStruct.dylib), main app:
//   src/game/HLSprite.cpp
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "HLSprite.h"
#include "platform/arena.h"

extern "C" {

void tpw_swapfly_run(uint64_t records, int32_t count) {
    SamsUtilities::USwapBlockStruct(reinterpret_cast<FLYBOY*>(static_cast<uintptr_t>(records)), count);
}

}  // extern "C"
