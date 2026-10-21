// CoasterTrainHelpers.h: file-local helpers that src/game/rides/CoasterTrain.cpp shares with CoasterQueue.cpp and
// CoasterSounds.cpp. Before the regroup, the functions using them shared a source file (2 files in all), which had one
// copy of each; that copy now lives here, static inline, so each file that includes this header keeps its own internal
// copy. The helpers it has in common with CoasterSoundsHelpers.h come from that header, which a file of both groups
// includes too.
#pragma once

#include <cstring>
#include "CoasterSoundsHelpers.h"   // the shared helpers

static inline void StoreFloat(uint32_t& word, float value) { std::memcpy(&word, &value, sizeof word); }
