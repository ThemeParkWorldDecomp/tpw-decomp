// CWorldSavvy::GetWorld (main app, 0x1014205c, 12 bytes; unit w129).
#include "WorldSavvy.h"

// A leaf that ignores `this` and answers the world the pointer at 0x1037ec0c holds (gWorld, which World_init.cpp assigns; not gGameWorld at 0x1037ebfc).
// A NULL gWorld is as invalid here as on the Mac.
CWorld& CWorldSavvy::GetWorld() const { return *gWorld; }
