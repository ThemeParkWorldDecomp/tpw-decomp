// PeepHelpers.h: file-local helpers that src/game/people/Peep.cpp shares with AdvisorScoring.cpp and Person.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>

static inline uint8_t* PBytes(void* p) { return static_cast<uint8_t*>(p); }
