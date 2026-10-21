// TPCoasterHelpers.h: file-local helpers that src/game/rides/TPCoaster.cpp shares with BumperRide.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>

// The guard of src/game/rides/TPCoaster.cpp, here too: the files include this header above their own code.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define TRAINS_SWAP_FILE_WORDS 1
#endif

static inline uint32_t Swap32(uint32_t v) {
#ifdef TRAINS_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
