// CParkAnalyser::GatesOpen (main app, 0x100e7028, 64 bytes; unit w129).
#include "WorldSavvy.h"
#include "analyser.h"

// The world is the one CWorldSavvy::GetWorld answers (the analyser is passed on and ignored); the gates are open when IsParkClosed (the whole word at
// world +0x1da710) is 0.
bool CParkAnalyser::GatesOpen() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    return world.IsParkClosed() == 0;
}
