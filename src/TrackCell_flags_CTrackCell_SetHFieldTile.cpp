// CTrackCell::SetHFieldTile (0x100a3bac), RefreshHFieldTile (0x100a3ec0) and QuickDelete (0x100a4e88), main app.
#include <cstdint>

#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "rsse.h"

int32_t gLLCC4840[3];   // 0x102e4840

void CTrackCell::SetHFieldTile() {
    if (gLLCC4818 != 0) {
        const int32_t t = static_cast<int32_t>(mType);
        if (t == 0xb || t == 0x10 || t == 0xc || t == 0x11) return;
        if (static_cast<int32_t>(GetMapCellEquiv()->mType) == 0x15) return;
    }

    uint32_t tile;
    int32_t rotation;
    if (static_cast<int32_t>(mType) <= 0 || static_cast<int32_t>(mType) == 5) {
        rotation = 0;
        tile = 0x37;
    } else {
        tile = GetTile(&rotation);
    }
    gLLCC4840[0] = static_cast<int32_t>(tile) >> 16;
    gLLCC4840[1] = static_cast<int32_t>(tile & 0xffff);
    gLLCC4840[2] = rotation;
    mTileData[0] = static_cast<uint32_t>(gLLCC4840[0]);
    mTileData[1] = static_cast<uint32_t>(gLLCC4840[1]);
    mTileData[2] = static_cast<uint32_t>(gLLCC4840[2]);

    const uint32_t t = static_cast<uint32_t>(mCellId) - 1;
    const int32_t x = static_cast<int32_t>(t & 0x7f);
    const int32_t y = static_cast<int32_t>(t >> 7);
    CELL_IndexType index = {mTileData[0], mTileData[1], mTileData[2]};
    const int32_t kind = static_cast<int32_t>(mTileData[0]);

    if (kind == 3) {
        if (mMeshInstance != 0) {
            LLCCM_DestroyKTrackMesh(mMeshInstance);
            mMeshInstance = 0;
        }
        if ((mFlags & 0x10) == 0 && mTileData[1] != 0) {
            mMeshInstance = LLCCM_CreateKTrackMesh(x, y, index, 0);
        }
    } else if (kind == 4) {
        if (mMeshInstance != 0) {
            LLCCM_DestroyWTrackMesh(mMeshInstance);
            mMeshInstance = 0;
        }
        if ((mFlags & 0x10) == 0 && mTileData[1] != 0) {
            mMeshInstance = LLCCM_CreateWTrackMesh(x, y, index, 0);
            if (mMeshInstance != 0) {
                SetCurRideAnim(gRideAnimSets[mMeshInstance], 0xc, 0, 7, 1.0f, 0);
            }
        }
    } else if (kind == 5) {
        if (mMeshInstance != 0 && static_cast<int32_t>(GetMapCellEquiv()->mType) != 0x15) {
            LLCCM_DestroyHoardingMesh(mMeshInstance);
            mMeshInstance = 0;
        }
        mMeshInstance = LLCCM_CreateHoardingMesh(x, y, index, 0);
    } else if (kind == 0) {
        if (mTileData[1] == 0x37 && mMeshInstance != 0) {
            LLCCM_DestroyKTrackMesh(mMeshInstance);
            mMeshInstance = 0;
        }
    }
}

void CTrackCell::RefreshHFieldTile() {
    const uint32_t t = static_cast<uint32_t>(mCellId) - 1;
    const int32_t x = static_cast<int32_t>(t & 0x7f);
    const int32_t y = static_cast<int32_t>(t >> 7);
    if (static_cast<int32_t>(GetMapCellEquiv()->mType) != 0x15) mMeshInstance = 0;

    CELL_IndexType index = {mTileData[0], mTileData[1], mTileData[2]};
    const int32_t kind = static_cast<int32_t>(mTileData[0]);
    if (kind == 3) {
        if ((mFlags & 0x10) == 0) mMeshInstance = LLCCM_CreateKTrackMesh(x, y, index, 1);
    } else if (kind == 4) {
        if ((mFlags & 0x10) == 0) mMeshInstance = LLCCM_CreateWTrackMesh(x, y, index, 1);
    } else if (kind == 5) {
        mMeshInstance = LLCCM_CreateHoardingMesh(x, y, index, 1);
    }
}

void CTrackCell::QuickDelete() {
    RemoveNeighbour(0xff);
    ModifyDirection(0);
    mFlags = 0;
    ModifyType(static_cast<CELL_MODES>(0));
    mParentID = 0;

    MapDelta d1 = gMapDeltaPlusY;
    CTrackCell* n = GetNeighbouringCell(d1, 1);
    if (n != nullptr) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->mParentID = 0;
    }

    MapDelta d2 = gMapDeltaPlusX;
    n = GetNeighbouringCell(d2, 1);
    if (n != nullptr) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->mParentID = 0;
    }

    MapDelta d3 = gMapDeltaPlusXPlusY;
    n = GetNeighbouringCell(d3, 1);
    if (n != nullptr) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->mParentID = 0;
    }
}
