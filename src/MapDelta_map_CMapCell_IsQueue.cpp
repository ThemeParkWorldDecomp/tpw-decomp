#include <cstddef>
#include <cstdint>
#include "analyser.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "lobby.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"

// Small predicates and accessors of CMapCell and CTrackCell (unit v51060).

// 0x1009c490, 36 bytes: the mode is 3 or 9.
int32_t CMapCell::IsQueue() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 3 || t == 9) return 1;
    return 0;
}

// 0x1009c44c, 28 bytes: the mode is 1.
int32_t CMapCell::IsPath() const {
    return static_cast<int32_t>(mType) == 1 ? 1 : 0;
}

// 0x1009c4dc, 28 bytes: the mode is 9.
int32_t CMapCell::IsEntrance() const {
    return static_cast<int32_t>(mType) == 9 ? 1 : 0;
}

// 0x1009c524, 28 bytes: the mode is 10.
int32_t CMapCell::IsExit() const {
    return static_cast<int32_t>(mType) == 0xa ? 1 : 0;
}

// 0x1009c568, 8 bytes: always 0.
int32_t CMapCell::IsBridge() const {
    return 0;
}

// 0x1009c5ec, 8 bytes: always 0.
int32_t CMapCell::IsLake() const {
    return 0;
}

// 0x1009c61c, 28 bytes: the mode is 4.
int32_t CMapCell::IsRide() const {
    return static_cast<int32_t>(mType) == 4 ? 1 : 0;
}

// 0x1009c660, 36 bytes: the mode is 0 (the original also tests 7 after a 0, which can never be true).
int32_t CMapCell::IsGrass() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t != 0) return 0;
    if (t == 7) return 0;
    return 1;
}

// 0x1009c6ac, 28 bytes: the mode is 0x1e.
int32_t CMapCell::IsWalkable() const {
    return static_cast<int32_t>(mType) == 0x1e ? 1 : 0;
}

// 0x1009c6f4, 48 bytes: bit 0x400 of the flag word.
int32_t CMapCell::IsWalkableInFirstPerson() const {
    return (mFlags & 0x400) != 0 ? 1 : 0;
}

// 0x1009c75c, 44 bytes: the mode is 5, 7 or 2.
int32_t CMapCell::IsNoGo() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 5 || t == 7 || t == 2) return 1;
    return 0;
}

// 0x1009c7b0, 28 bytes: the mode is 0x15.
int32_t CMapCell::IsCoasterPylon() const {
    return static_cast<int32_t>(mType) == 0x15 ? 1 : 0;
}

// 0x1009c8c4, 124 bytes: the neighbour byte when exactly two in a line, else 0.
uint8_t CMapCell::IsStraight() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 0x10) != 0) return neighbours;
    if ((neighbours & 4) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// 0x1009ca44, 44 bytes: the track cell's answer for the cell's track twin.
int32_t CMapCell::IsTrackX() const {
    return GetTrackCellEquiv()->IsTrackX();
}

// 0x1009bbe8, 8 bytes.
uint16_t CMapCell::GetFlag() {
    return mFlags;
}

// 0x1009bbb8, 8 bytes.
void CMapCell::SetFlag(uint16_t flag) {
    mFlags = flag;
}

// 0x1009d07c, 68 bytes: destroys the instance mesh, if any.
void CMapCell::RemoveInstanceMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyQMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

// 0x1009d860, 8 bytes.
int16_t CMapCell::GetPylonIndex() const {
    return mPylonIndex;
}

// 0x1009d898, 8 bytes.
void CMapCell::SetPylonIndex(int16_t index) {
    mPylonIndex = index;
}

// 0x10099930, 16 bytes.
void CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) {
    mParentID = id.mIndex;
}

// 0x100a0b68, 16 bytes.
void CTrackCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) {
    mParentID = id.mIndex;
}

// 0x100a3574, 60 bytes: the mode is 0xb, 0xd, 0x10, 0x12 or 0x19.
int32_t CTrackCell::IsTrack() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 0xb || t == 0xd || t == 0x10 || t == 0x12 || t == 0x19) return 1;
    return 0;
}

// 0x100a3628, 36 bytes: the mode is 0xb or 0x10.
int32_t CTrackCell::IsJustTrack() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 0xb || t == 0x10) return 1;
    return 0;
}

// 0x100a3a38, 8 bytes.
void CTrackCell::SetSegmentNumber(int16_t number) {
    mSegmentNumber = number;
}

// 0x100a4048, 68 bytes.
void CTrackCell::RemoveInstanceMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyKTrackMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

// 0x100a5098, 68 bytes.
void CTrackCell::DeleteMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyKTrackMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

// 0x100a66b0, 12 bytes.
int32_t CLBLobby::IsKeyDown(uint32_t flag) {
    return static_cast<int32_t>(static_cast<uint32_t>(mF1C) & flag);
}

// 0x10090b2c, 16 bytes.
void __sinit_LLCellRide_cpp() {
    gLLCCE092 = 0;
}

// 0x1008f9c8, 52 bytes: the four limits of the hoarding walk.
void LLCR_GetHoardingExtents(int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    *a = gLLCCHoardMinX;
    *b = gLLCCHoardMinY;
    *c = gLLCCHoardMaxX;
    *d = gLLCCHoardMaxY;
}

// 0x1008f2f4, 16 bytes.
void LLCC_ResetPylonMove() {
    gLLCCE090 = 0;
}
