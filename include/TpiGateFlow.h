// TpiGateFlow.h: the TPI Worlds and Rides mod's walks outside the park gate (TPW_MOD_HOOK tpi_worlds): where a visitor who
// gets off the bus waits to cross, and where a visitor who leaves waits for the bus, in the mod's worlds.
// The game's own walks (CPerson's constructor, BusStopCells and CWorld::ArrivalVehicle_PeopleWaitingToLeave) were made for
// Theme Park World's entrance: the stops on a pavement row with a second row two rows above it, and the crossing's bus side
// on that pavement. Theme Park Inc's entrance (the converted Standard.sam's fixed positions over the zone's own Base.map)
// is laid out otherwise: a pavement two rows deep with bus stop A and bus stop B on its upper row, the map's edge above it,
// and the road between it and the gate's plaza, which the walkers go round by the pavement at the east end. There the cells
// two rows above stop A and the crossing's bus side (on the road) are not walkable, so a visitor leaving waits for ever on
// the plaza in front of the gate, and a visitor arriving gets no walk to the crossing and wanders in without paying.
// In the mod's worlds the second row of the stop's cells is the row below the stop, and the crossing's bus side is in the
// crossing's column on that row too: both on the pavement. Header only and pure, so the host test sees what the game does.
#pragma once

