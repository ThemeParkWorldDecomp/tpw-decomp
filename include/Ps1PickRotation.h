// Ps1PickRotation.h: the ps1-rides mod's starting turn of its rides picked from Buy Ride (TPW_MOD_HOOK ps1_pick_rotation).
// A PC ride's footprint has its entrance on its south side, so at the default camera (north at the top) a ride picked
// from Buy Ride comes up with its entrance at the bottom. The PlayStation rides the mod converts have theirs on the
// north side of their footprint (the first line of Info.Shape), so they came up with the entrance at the top. With the
// hook on, picking one of the mod's rides from Buy Ride (PlaceRide, src/game/ui/ui_ridepanel.cpp) adds half a turn to
// the ride's rotation, the turn measured in game that puts its entrance where a PC ride's is. Nothing else changes: the
// converted rides, a saved park and the rides already placed keep their rotation, and the player turns the held ride
// as before. The original keeps the rotation of a ride still held when another is picked; so does the hook, the half
// turn it added for the ride held taken away first, so that picking again does not add it twice. Only with the port's
// fixes: off (the default) or in a build without them, every ride starts as the original's.
#pragma once

