// CMapCell::SetHFieldTile (main app, SimTheme Park 2.04b3, MapCell.cpp; unit w139), 0x1009cdb4, 384 bytes.
//
// A cell whose mode is 0 or less, or 5, gets the blank tile 0x37 with rotation 0; any other mode asks GetTile for its tile word and
// rotation. The tile word is split into (word >> 16 arithmetic, word & 0xffff, rotation) in the cell system's scratch tile gLLCC4824, and
// the cell's mTileData receives a copy. The cell's place comes from its id (32 bit arithmetic: id 0 gives x = 0x7f, y = 0x1ffffff), read
// after the stores. A queue tile (kind 2) replaces the cell's mesh by a new queue mesh (the old one destroyed first) and then repaints the
// height field with the plain tile {0, 8, 0}; any other tile only drops the old mesh and repaints with the cell's own tile data.
#include "MapCell.h"

#include <cstdint>

#include "LLCellSystem.h"

void CMapCell::SetHFieldTile() {
    uint32_t tile;
    int32_t rotation;
    if (static_cast<int32_t>(mType) <= 0 || static_cast<int32_t>(mType) == 5) {
        tile = 0x37;
        rotation = 0;
    } else {
        tile = GetTile(&rotation);
    }
    gLLCC4824.mKind = static_cast<uint32_t>(static_cast<int32_t>(tile) >> 16);
    gLLCC4824.mIndex = tile & 0xffffu;
    gLLCC4824.mRotation = static_cast<uint32_t>(rotation);
    mTileData[0] = gLLCC4824.mKind;
    mTileData[1] = gLLCC4824.mIndex;
    mTileData[2] = gLLCC4824.mRotation;

    const uint32_t id = mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    if (mTileData[0] == 2) {
        if (mMeshInstance != 0) {
            LLCCM_DestroyQMesh(mMeshInstance);
            mMeshInstance = 0;
        }
        const CELL_IndexType index = {mTileData[0], mTileData[1], mTileData[2]};
        mMeshInstance = LLCCM_CreateQMesh(x, y, index, 0);
        gLLCC4824.mKind = 0;
        gLLCC4824.mIndex = 8;
        gLLCC4824.mRotation = 0;
        SingleHFieldTextureChange(x, y, &gLLCC4824);
    } else {
        if (mMeshInstance != 0) {
            LLCCM_DestroyQMesh(mMeshInstance);
            mMeshInstance = 0;
        }
        SingleHFieldTextureChange(x, y, reinterpret_cast<const CELL_IndexType*>(mTileData));
    }
}
