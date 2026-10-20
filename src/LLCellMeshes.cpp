// The four ride-mesh instancers of the cell system (LLCellMouse.cpp of the main app; unit w120): LLCCM_CreateQMesh (0x10081c58, 224 bytes),
// LLCCM_CreateKTrackMesh (0x10081fb8, 284), LLCCM_CreateWTrackMesh (0x10082350, 364) and LLCCM_CreateHoardingMesh (0x10082720, 284).
// CMapCell::SetHFieldTile and CTrackCell::SetHFieldTile call them with restore 0 to put a queue, track or hoarding mesh on a cell, and the
// RefreshHFieldTile functions with restore 1 to find the mesh again after a load. Each answers the instance number, which the caller keeps
// in the cell. The Q instancer is the plain form; the others move the ride's corner cell for the quarter turns. All four place the mesh with
// the flags 0x33a (place from the parameters, drawing on, skirt deform, world bounding area, coaster).
#include "LLCellSystem.h"

#include "TPEngine.h"
#include "rsse.h"

namespace {

// The ride's turn: 360 less the tile's rotation (a rotation of 0 gives 0, not 360).
int32_t RideRotation(const CCellBase::CELL_IndexType& index) {
    int32_t rotation = static_cast<int32_t>(0x168u - index.mRotation);
    if (rotation == 0x168) rotation = 0;
    return rotation;
}

// The corner cell of a mesh that is turned by 90, 180 or 270 degrees moves one cell (180 moves both ways).
void ShiftForRotation(int32_t rotation, int32_t& x, int32_t& y) {
    if (rotation == 0xb4) {
        x = x + 1;
        y = y + 1;
    } else if (rotation < 0xb4) {
        if (rotation == 0x5a) y = y + 1;
    } else if (rotation == 0x10e) {
        x = x + 1;
    }
}

// Makes the instance (restore 0) or finds the restored one (restore nonzero). The parameters' last word is the address of the size pair
// (the ride's width and depth in cells), which both calls read through the record.
uint32_t PlaceMesh(int32_t x, int32_t y, int32_t rotation, const sLLCMMesh& mesh, int32_t* sizePair, int32_t restore) {
    sFixedRideParams params;
    params.mX = x;
    params.mZ = y;
    params.mRotation = rotation;
    params.mF0C = sizePair;
    if (restore != 0) {
        return RestoreRideInstance(0, mesh.mRideType, 0x33a, &params, mesh.mTimeControls, nullptr, 0, 0, 0);
    }
    return InstanceRide(mesh.mRideType, 0x33a, &params, mesh.mTimeControls, 0, nullptr, 0, 0, 0xffffffff, 0);
}

}  // namespace

// 0x10081c58 (224 bytes): no cell shift, the one-cell size pair and the queue table.
uint32_t LLCCM_CreateQMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    return PlaceMesh(x, y, rotation, gLLCMQMeshes[index.mIndex], gLLCMQSize, restore);
}

// 0x10081fb8 (284 bytes): the go-kart track table.
uint32_t LLCCM_CreateKTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    return PlaceMesh(x, y, rotation, gLLCMKTrackMeshes[index.mIndex], gLLCMTrackSize, restore);
}

// 0x10082350 (364 bytes): the water track table; a new or restored instance (not 0) starts animation 12 of its set, looping (7 is the loop
// mode), at speed 1.
uint32_t LLCCM_CreateWTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    const uint32_t instance = PlaceMesh(x, y, rotation, gLLCMWTrackMeshes[index.mIndex], gLLCMTrackSize, restore);
    if (instance != 0) {
        SetCurRideAnim(gRideAnimSets[instance], 0xc, 0, 7, 1.0f, 0);
    }
    return instance;
}

// 0x10082720 (284 bytes): the hoarding table.
uint32_t LLCCM_CreateHoardingMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    return PlaceMesh(x, y, rotation, gLLCMHoardingMeshes[index.mIndex], gLLCMTrackSize, restore);
}
