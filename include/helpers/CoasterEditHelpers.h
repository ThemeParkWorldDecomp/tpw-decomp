// CoasterEditHelpers.h: file-local helpers that src/game/rides/CoasterEdit.cpp shares with CoasterTrack.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TPCoaster.h"

// The brace's pylon record is a sCoasterPylon on the host (native pointers): its control is the word at +0x18.
static inline sPylonControl* ControlOf(const sTrackBraceData* pylon) { return reinterpret_cast<const sCoasterPylon*>(pylon)->mControl; }
