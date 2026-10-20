// CTrackCell::IsFlag (main app, SimTheme Park 2.04b3, 0x100a2918, 32 bytes; unit w206): whether any bit of `flag` (its low halfword) is set in the
// cell's flag word. The twin of CMapCell::IsFlag (src/MapCell_flags.cpp, unit w201).
#include <cstdint>

#include "TrackCell.h"

int32_t CTrackCell::IsFlag(uint16_t flag) const { return (mFlags & flag) != 0 ? 1 : 0; }
