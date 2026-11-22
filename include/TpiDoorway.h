// TpiDoorway.h: the TPI Worlds and Rides mod's doorway floors (TPW_MOD_HOOK tpi_worlds, LoadM3D2Header and
// SRenderListEntry::CalcRenderEntry). Theme Park Inc's shops and rides darken their entrances with a floor textured
// doorway_alpha.tga (black, its alpha a gradient from the door inwards, blended: TPW_MOD_HOOK tpi_worlds in
// TGALoadAndConvert), laid a hair above the mesh's opaque base and drawn after it: 0.03 world units in Polar Zone's
// umbrella shop, 0.002 to 0.6 in the others. Each projected vertex's depth is rounded in single precision (the
// projection's clip z and w, then CMatrixPowerPC::Project's depth curve), and the curve's far slope (gei_Display.mZ6C: 7
// at zoom 130, 12 at 250, 22 at 500) multiplies that rounding, so the floor and the base each land up to one or two
// times mZ6C steps of the 24-bit depth buffer off their place, while the umbrella shop's floor is only about 1800 / zoom
// steps above its base. Up to the original's zoom limit (130) that floor still wins the depth test; at the port's farther
// zooms (to 500 with the free camera, 800 with "Zoom out further") the base wins it on whole floor triangles, depending
// on the exact camera, so the ground showed through the entrance as a white wedge that came and went as the camera
// turned (the maintainer, 2026-11-20, Polar Zone's umbrella shop at about zoom 250). In the mod's worlds such a floor is drawn
// kTpiDoorwayFarSteps times mZ6C depth steps nearer the camera than its place, after its projection, so it wins the test
// at every zoom and angle; its geometry, texture, colour, sort depth and draw order stay Theme Park Inc's. Header only
// and pure, so the host test (tests/host_tpi_doorway.cpp) sees what the game runs.
#pragma once

