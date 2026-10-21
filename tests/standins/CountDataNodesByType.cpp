// The weak stand-ins of the unit library tpw_unit_CountDataNodesByType (cmake/units/CountDataNodesByType.cmake), as
// src/advisor_leaves_CountDataNodesByType.cpp of before the regroup (v5L30b) defined them, unchanged. The regroup
// put each function into the src file of its original source file, where these defaults would sit next to the real
// definitions, so it dropped them; the unit library compiles them from here, so that it builds what it built before.

#include <cmath>
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"

// Weak defaults for callees that no translation provides yet (the test library defines the real fakes).
__attribute__((weak)) uint32_t CPerson::GetHeadSpriteID() { return 0; }
