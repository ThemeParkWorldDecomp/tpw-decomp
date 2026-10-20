// CMapCell::IsFlag (main app, SimTheme Park 2.04b3, 0x1009bb70, 32 bytes; unit w201): whether any bit of `flag` (its low halfword) is set in the cell's
// flag word.
#include <cstdint>

#include "MapCell.h"

int32_t CMapCell::IsFlag(uint16_t flag) const { return (mFlags & flag) != 0 ? 1 : 0; }
