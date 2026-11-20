// TpiParkEdge.h: the TPI Worlds and Rides mod's soft edge of the park's land (TPW_MOD_HOOK tpi_worlds, CHField::Execute).
// Land the park does not own is darker: RecalculateParkDarkening (src/game/park/TPHField.cpp) gives each height field cell
// over such land quad bit 0x400. The game shows it two ways. The meshes (the river, its banks, the scenery) are lit per
// vertex, each vertex darker (its normal scaled to 0.225) when the cell under it has the bit (UpdateHFieldDarkeningNormals),
// so across the edge the light fades over one triangle, about one cell wide. The height field's own ground is darkened per
// cell (CHField::Execute halves the colour of all four corners of a cell with the bit), so its edge is a hard step. In Theme
// Park World's worlds the park's hoarding stands on that step and hides it; the mod's worlds have no hoarding (TPW_MOD_HOOK
// tpi_worlds in LLCCM_CreateHoardingMesh), so the step showed as a sharp line, unlike the soft fade along the river beside
// it. In the mod's worlds the ground takes the meshes' rule: each corner is darkened from the cell under its grid point, the
// cell it is the first corner of (the last row's and column's points, which are no cell's first, take the cell before),
// as UpdateHFieldDarkeningNormals picks the cell under a vertex, and the colours between the corners are blended as the
// meshes' are, so the ground fades over one cell, beside the river's fade. Header only and pure, so the host test sees
// what the game does.
// The meshes the converter takes from Theme Park Inc's levels (the canyons' cliffs, the tunnel's top and mouths, the
// sculptures) have vertices meant to lie on a cell's edge that lie a hair before it (up to about 0.01 world units, a
// thousandth of a cell), so UpdateHFieldDarkeningNormals took the cell before the edge for them and the mesh's light
// stepped one cell away from the ground's fade: a hard line along the edge of the park's land. In the mod's worlds a
// vertex within a thousandth of a cell of a grid line counts as on it (TpiParkEdgeVertexCell), so it takes that grid
// point's cell, as the ground's corner does.
#pragma once


