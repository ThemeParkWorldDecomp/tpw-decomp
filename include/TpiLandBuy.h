// TpiLandBuy.h: the TPI Worlds and Rides mod's land buying around the park gate (TPW_MOD_HOOK tpi_worlds,
// LLCC_IsHoardingEditLegal and LLCC_FillInsideOfPark in src/game/park/LLCellSystem.cpp).
// The park's hoarding is an open fence from one gate post, (gHoardingRight, gHoardingBottom), round the park to the other,
// (gHoardingLeft, gHoardingTop); the gate closes it. A Buy Land rectangle that takes in the gate makes the new fence a
// small loop round the gate alone, and the fill of the park's inside then scans out of the level: the Mac reads through
// the missing cells, the port crashes on them. The original never gets there: the land outside each of Theme Park World's
// gates is freeform, and two freeform cells in a row along a side make an edit illegal. In the mod's worlds a freeform cell
// does not count (so the land past Theme Park Inc's bridges and obstacles can be bought), which let such a rectangle
// through. So in the mod's worlds an edit that meets the gate (either post, on its edge or inside it, or the opening
// between them) is refused, and the fill's scans stop before a cell whose square of four leaves the level, as a guard.
// Header only and pure, so the host test sees what the game does.
#pragma once

