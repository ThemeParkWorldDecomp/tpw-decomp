// The Bullfrog library's rectangle helpers (bullfrog symbols shared, LbRect_* in the LbDraw range). A TbRect is four
// 32-bit words, left and top inside, right and bottom outside (include/TbGraphicFile.h). LbRect_Intersection
// (0x10029ad0) and LbRect_Empty (0x10029cd4) are declared there, because the picture loader calls them; they are
// defined with LbRect_Normalise in src/bfl_lbdraw_leaves.cpp.
#pragma once
#include "TbGraphicFile.h"

// 0x10029d14: puts left before right and top before bottom by swapping each pair that is the wrong way round. The
// original swaps with three exclusive-ors; the result is the same.
void LbRect_Normalise(TbRect* rect);

// Unit w224 (src/LbDraw_text.cpp)
int32_t LbRect_Width(const TbRect* rect);  // bullfrog 0x1002998c, 16 B
int32_t LbRect_Height(const TbRect* rect); // bullfrog 0x100299c8, 16 B
