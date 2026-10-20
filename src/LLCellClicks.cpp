// The cell system's click stack and buy land list resets (main app, LLCellSystem.cpp range; unit w112a). Kept out of
// LLCellSystem.cpp, which unit w111's test builds with fakes of these names.
#include "LLCellSystem.h"

int32_t gLLCCClickStackCount;

// 0x10091dd4 (16 bytes): empties the click stack (its entry count is the one word the other click stack functions share).
void LLCC_SystemResetClickStack() { gLLCCClickStackCount = 0; }

// 0x10096190 (4 bytes): a bare blr in the original, so nothing is reset.
void LLCC_ResetBuyLandList() {}
