// TPRide.cpp (main app 0x1005b8fc, unit w114): the level's procedural texture set.
#include <cstdio>

#include "TPEngine.h"
#include "TPMain.h"
#include "rsse.h"
#include "tb_hfield.h"

// Loads the procedural textures from "<mesh folder>base.lnd" (just "base.lnd" when GetMeshPath answers NULL) with the engine's PT_Load
// (its answer is dropped), then, reading gGameOptions after PT_Load, makes the quads' orientation flags match the textures being on
// (bit 0x4000: CorrectOrientationFlagsForDiffProceduralMode(1)) or turns the procedural textures off (PT_Disable). The Mac's sprintf
// into the 1024 byte buffer is unbounded; this one is cut at the buffer.
void LoadProceduralTextureSet(int32_t mode) {
    char path[1024];
    const char* folder = GetMeshPath();
    if (folder != nullptr) std::snprintf(path, sizeof path, "%sbase.lnd", folder);
    else std::snprintf(path, sizeof path, "base.lnd");
    PT_Load(path, mode);
    if ((gGameOptions & 0x4000) != 0) CorrectOrientationFlagsForDiffProceduralMode(1);
    else PT_Disable();
}
