// TpiFenceBuild.h: the cells of the TPI Worlds and Rides mod's worlds that the level's own fences, walls and banks stand on
// (TPW_MOD_HOOK tpi_worlds). Theme Park Inc models some of its park's edges into the terrain (Polar Zone's wooden fence
// down the east side, Land of Invention's wooden fence along the south) and marks the two cells either side of the fence
// line, and the edges of its canyons and banks, with Base.map attribute bit 1, the bit Theme Park World gives its own
// unbuildable features (Wonderland's trench, the map's rim). Theme Park World never reads the bit for building: its
// features stand on hidden terrain or land no park owns. The converted fences stand on plain terrain, so once Buy Land,
// which takes land past Theme Park Inc's EndHoardings fence, owns both sides, a ride, shop or path could stand right
// across a fence. In the mod's worlds a cell with the bit refuses every placement that builds; demolishing, the land
// tools and the hoarding keep the Mac's answer. Header only and pure, apart from the theme flag, so the host test sees
// what the game does.
#pragma once

