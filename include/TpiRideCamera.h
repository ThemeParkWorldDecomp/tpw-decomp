// TpiRideCamera.h: the TPI Worlds and Rides mod's ride camera node (TPW_MOD_HOOK tpi_worlds, SetupRideRideMatrixAndSuch).
// The first person view of a ride sits at an embedded node of the ride's mesh with flag 0x1000, the camera node; with none
// the view does not open. Theme Park Inc's Polar zone ice skates (Bumper Ride, ride 6030) have a camera mesh of their own
// whose two embedded entries are a rider's head (flag 0x80) and an accessory (0x400), and no camera node, so the ride's
// "ride it" button did nothing. In the mod's worlds a mesh with no camera node puts the view at its first rider's head:
// the head with id 1, else the first head. Header only and pure, so the host test sees what the game does.
#pragma once

