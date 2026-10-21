// analyserHelpers.h: file-local helpers that src/game/park/analyser.cpp shares with World.cpp. Before the regroup, the
// functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "WorldSavvy.h"

static inline const CWorldSavvy* Savvy(const CParkAnalyser* analyser) { return reinterpret_cast<const CWorldSavvy*>(analyser); }

// The research lab of the world (the const twin, 0x1013ec58).
static inline const CResearchLab* LabOf(const CParkAnalyser* analyser) { return static_cast<const CWorld&>(Savvy(analyser)->GetWorld()).GetResearchLab(); }
