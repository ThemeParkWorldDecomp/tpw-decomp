// The map reset (main app of SimTheme Park 2.04b3; the inventory files LLCC_ResetMap under LLCellSystem.cpp, HFieldCellStatus under
// TPRide.cpp, RemoveNeighbour and ModifyDirection under LLCellMouse.cpp and the cell resets under unrelated names; unit w112c):
// LLCC_ResetMap puts every cell of the map and of the track layer back to its start state (the cell's place comes from its id, the
// start state from the height field and, for the map cells, the attribute map).
#include "CellBase.h"
#include "LLCellSystem.h"
#include "LevelSystems.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "World.h"
#include "m3d2.h"

// 0x100916a8 (204 bytes): rows first, a map cell then a track cell for each place. The place is the 16 bit number x + (y << 7) + 1
// (the same as the cell's id), so the cells are indexed by that number less one, in 32 bit arithmetic: number 0, which a 16 bit wrap
// gives, is the cell before the first. gGameWorld is read again before each call. The game passes (0x80, 0x80).
void LLCC_ResetMap(int32_t width, int32_t height) {
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const uint16_t place = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
            (gGameWorld->mMapCells + (place - 1))->Reset(true);
            (gGameWorld->mTrackCells + (place - 1))->Reset();
        }
    }
}

// 0x1009cb48 (580 bytes). The cell's place comes from its id (32 bit arithmetic, so id 0 gives x = 0x7f and y = 0x1ffffff). The
// stores come in the listing's order: the callees are stand-ins that look at the cell. The attribute byte is read after the first
// stores and before HFieldCellStatus. When that answers 0 (the cell is outside the height field or its quad is flagged) the mode is 2
// (attribute 2), 0x1e (attribute 0x80) or 7, and the tile is set when `full`. Otherwise 5, 2 or 0x1e are stored and at once
// overwritten (dead stores) by the attribute 8 test: mode 1 with CalculateNeighbours and the flags set to 0x20, or mode 0.
void CMapCell::Reset(bool full) {
    const uint32_t id = mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    RemoveNeighbour(0xff);
    ModifyDirection(0);
    mParentID = 0;
    mFlags = 0;
    mTileData[0] = 0;
    mTileData[1] = 0;
    mTileData[2] = 0;
    const uint8_t attribute = mStatusFlags;
    if (HFieldCellStatus(x, y) != 0) {
        if ((attribute & 1) != 0) {
            mType = static_cast<CELL_MODES>(5);
        } else if ((attribute & 2) != 0) {
            mType = static_cast<CELL_MODES>(2);
        } else if ((attribute & 0x80) != 0) {
            mType = static_cast<CELL_MODES>(0x1e);
        }
        if ((attribute & 8) != 0) {
            mType = static_cast<CELL_MODES>(1);
            CalculateNeighbours(mType, 0, 0, full);
            mFlags = 0x20;
        } else {
            mType = static_cast<CELL_MODES>(0);
        }
        if ((mFlags & 0x20) == 0) {
            mFlags = static_cast<uint16_t>(mFlags | 0x40);
            if (full) SetHFieldTile();
        }
    } else {
        if ((attribute & 2) != 0) {
            mType = static_cast<CELL_MODES>(2);
        } else if ((attribute & 0x80) != 0) {
            mType = static_cast<CELL_MODES>(0x1e);
        } else {
            mType = static_cast<CELL_MODES>(7);
        }
        if ((mFlags & 0x20) == 0) mFlags = static_cast<uint16_t>(mFlags | 0x40);
        if (full) SetHFieldTile();
    }
    if ((attribute & 0x10) != 0) mFlags = static_cast<uint16_t>(mFlags | 0x80);
    if ((attribute & 4) != 0) mFlags = static_cast<uint16_t>(mFlags | 0x400);
    mMeshInstance = 0;
    mOverlapCounter = 0;
}

// 0x100a3a74 (272 bytes). The mode is 0 when HFieldCellStatus answers nonzero and 7 otherwise (ModifyType). The cell
// system's flag byte is 1 for the second RemoveNeighbour (so a track cell clears its hoarding mask) and ends 0, whatever it was.
void CTrackCell::Reset() {
    const uint32_t id = mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    if (HFieldCellStatus(x, y) != 0) {
        ModifyType(static_cast<CELL_MODES>(0));
    } else {
        ModifyType(static_cast<CELL_MODES>(7));
    }
    RemoveNeighbour(0xff);
    gLLCC4818 = 1;
    RemoveNeighbour(0xff);
    gLLCC4818 = 0;
    ModifyDirection(0);
    mFlags = 0;
    mParentID = 0;
    mMeshInstance = 0;
    mOverlapCounter = 0;
    mTileData[0] = 0;
    mTileData[1] = 0;
    mTileData[2] = 0;
    mSegmentNumber = -1;
}

// 0x1005b7a8 (112 bytes). The Mac's arithmetic is 32 bit: the quad's address is the table plus ((x + y * width) << 2) wrapped in
// 32 bits, and only the first half word of the quad is read.
int32_t HFieldCellStatus(int32_t x, int32_t y) {
    tag_sM3D2Header* const header = gHeightfieldHeader;
    if (header == nullptr) return 1;
    const sHField* const field = header->mHField;
    if (field == nullptr) return 1;
    const uint32_t width = field->mF18[0];
    if (static_cast<uint32_t>(x) >= width) return 0;
    if (static_cast<uint32_t>(y) >= field->mF18[1]) return 0;
    const uint32_t offset = (static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * width) << 2;
    const sHFieldQuad* const quad =
        reinterpret_cast<const sHFieldQuad*>(reinterpret_cast<const uint8_t*>(field->mQuads.get()) + offset);
    return (quad->mF00 & 1) != 0 ? 0 : 1;
}

// 0x10081678 (60 bytes). With the cell system's flag set a track cell (kind 2) clears the bits in its second mask instead.
void CCellBase::RemoveNeighbour(uint8_t mask) {
    if (gLLCC4818 != 0 && mKind == 2) {
        mHoardingNeighbours = static_cast<uint8_t>(mHoardingNeighbours & ~mask);
        return;
    }
    mNeighbours = static_cast<uint8_t>(mNeighbours & ~mask);
}

// 0x10081810 (8 bytes).
void CCellBase::ModifyDirection(uint8_t direction) { mDirection = direction; }
