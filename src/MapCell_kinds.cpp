// CMapCell's flag clearing and two terrain tests (main app, SimTheme Park 2.04b3; unit w206): UnSetFlag 0x1009bc18 (16 bytes), IsWater 0x1009c408
// (28 bytes) and IsFreeform 0x1009c59c (36 bytes). The tests answer a word, 1 or 0, from the cell's mode alone.
#include <cstdint>

#include "MapCell.h"

// 0x1009bc18, 16 bytes: clears the bits of flag in the flag word (only the low halfword is stored).
void CMapCell::UnSetFlag(uint16_t flag) { mFlags = static_cast<uint16_t>(mFlags & ~flag); }

// 0x1009c408, 28 bytes: the cell's mode is 2.
int32_t CMapCell::IsWater() const { return mType == 2 ? 1 : 0; }

// 0x1009c59c, 36 bytes: the cell's mode is 7, else 0x1e.
int32_t CMapCell::IsFreeform() const {
    if (mType == 7) return 1;
    return mType == 0x1e ? 1 : 0;
}
