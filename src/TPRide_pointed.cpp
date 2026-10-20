// TPRide.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: GetPointedAtRide (0x10067254, 12 bytes) and
// LockPointedAtRide (0x10067288, 52 bytes).
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ui_main.h"

tag_sRideAnimSet* GetPointedAtRide() { return gHighlightedRide; }

// Keeps the highlight when asked to and a ride is highlighted; otherwise lets it go.
void LockPointedAtRide(int32_t flag) { gRideHighlightKeep = (flag != 0 && gHighlightedRide != nullptr) ? 1 : 0; }
