// ResearchLabHelpers.h: file-local helpers that src/game/buildings/ResearchLab.cpp shares with World.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "WorldSavvy.h"

// The park analyser of the world the lab's GetWorld answers (the const getter, 0x1013eb38; the lab is a CWorldSavvy to the Mac).
static inline const CParkAnalyser* AnalyserOf(const CResearchLab* lab) {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(lab)->GetWorld();
    return world.GetParkAnalyser();
}
