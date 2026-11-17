// TpiOutside.h: the TPI Worlds and Rides mod's outside features (TPW_MOD_HOOK tpi_worlds). Theme Park Inc's scenery round
// each park (the waterfall, water wheel and ponds of the "water" item, the ticket booths, the helipad, the bunting and
// each world's installations, penguins or seals) are items of their own, flagged Info.IsOutsideFeature 1, shown in no
// build menu (Info.WhichUIType 4) and modelled where they stand (Info.DontApplyOffset 1), which Theme Park Inc placed when
// a park started. Theme Park World places only its gates and lights that way (CWorld::SetupNewLevel), so the converted
// worlds lacked them: no decor outside the park, and the ponds of the level's ground showed the clear colour (flat cyan)
// with no water item over them.
// The converter (assets/tpi_convert.cpp, TpiOutsideSam) writes the converted ids of a world's outside features into the
// world as TpiOutside.sam, one "Outside <id>" line each; CWorld::SetupNewLevel reads it (TpiParseOutside) into
// gTpiOutside and places each one as it places the gates (a feature object on map cell 1). A park keeps its objects, so
// one saved before converter 28 has none: on loading, each outside feature the park does not have is placed then
// (TpiOutsideToPlace, RepairTpiOutsideFeatures in src/game/save/SaveLoad.cpp), and a second load changes nothing.
// Header only and pure, so the host test sees what the game does.
#pragma once

