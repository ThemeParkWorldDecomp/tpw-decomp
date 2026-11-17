// TpiGreatLake.h: the TPI Worlds and Rides mod's Great Lake (TPW_MOD_HOOK tpi_worlds): in the Land of Invention the
// Great Lake (Theme Park Inc's vict_hs, a bumper ride of kind -11 with boats) is placed in front of the lake, and its
// boats leave the ride's plot, sail on the lake and come back to the plot to unload.
// Theme Park Inc's bumper.sam for the ride gives the circles its boats may sail in (ColObjects) and the buoys they sail
// to (Buoys), both in map units of 10 a cell, laid out on the lake in front of the ride's fixed spot. Theme Park World's
// kind -11 has one circle on the ride's plot and a ring of 8 buoys in it. The converter (assets/tpi_convert.cpp,
// TpiLakeSam) writes the lake's water cells, its water level and those circles and buoys into the world as TpiLake.sam;
// CWorld::SetupNewLevel reads it (TpiParseLake) into gTpiLake.
// Placement (LLCR_PlaceRideBlock): the ride may be placed only where every cell behind its footprint (the row past the
// edge away from its entrance and exit) is lake water (TpiLakeBackOnWater).
// The ride (BMP_CreateRide): its circle on the plot is joined to Theme Park Inc's circles on the lake by a chain of
// circles (TpiLakeBuild): one on the plot's back row, then one a cell along paths of water cells to each of Theme Park
// Inc's circles. Every circle stays within the lake's water and the plot, so a boat's centre never goes over land
// (BMP_InsideArea keeps a boat inside the circles). The ring's buoys move to Theme Park Inc's buoys.
// Steering (SetNavRoute, TpiLakeAim): a boat whose target is in another circle steers to the middle of the overlap of
// its circle and the next circle on the shortest chain to the target's circle, so it never runs into the shore.
// Unloading (BMP_AI): when the ride's time is up a boat sails back to the plot's circle before it unloads.
// Drawing (BMP_DrawCarsForRide): a boat over water floats at the water level, a little above it so its wake shows.
// Header only and pure, so the host test sees what the game does.
#pragma once

