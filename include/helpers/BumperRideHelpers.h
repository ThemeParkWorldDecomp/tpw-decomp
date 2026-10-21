// BumperRideHelpers.h: file-local helpers that src/game/rides/BumperRide.cpp shares with TPCamera.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstring>

template <class T>
static inline T& At(void* base, int32_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}
