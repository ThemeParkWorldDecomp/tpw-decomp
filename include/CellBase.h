// CCellBase (the base of CMapCell and CTrackCell; the inventory files its code under Entertainer.cpp, unreliable; main app).
// PARTIAL (units w113 and w112c): the two grid registrations CWorld::CWorld makes and the six globals they fill (w113), the two
// members the cell resets call (w112c) and the data bytes 0 to 0x23 both cell classes share (CMapCell::Serialise and
// CTrackCell::Serialise call CCellBase::Serialise first; the names are the serialisers'). No virtuals: the Mac object has no vtable.
#pragma once
#include <cstddef>
#include <cstdint>

class CMapCell;
class CTrackCell;

// The mode (the type) of a cell. The enumerators are not known; the values the resets store are 0, 1, 2, 5, 7 and 0x1e (the
// predicates CMapCell::IsGrass == 0, IsPath == 1, IsWater == 2 and IsWalkable == 0x1e suggest their meaning).
enum CELL_MODES : int32_t {};

class CCellBase {
public:
    // 0x10081678 (60 bytes, src/game/park/CellBase.cpp): clears the bits of `mask` in the neighbour mask. The cell system's flag byte
    // (gLLCC4818) set and a track cell (kind 2) turn it to the hoarding mask at +0x22 instead.
    void RemoveNeighbour(uint8_t mask);
    // 0x10081810 (8 bytes): stores the direction byte.
    void ModifyDirection(uint8_t direction);

    // 0x100f4780 (36 bytes, src/game/park/CellBase.cpp), a static member (the call sites pass the base in r3): records the map grid. The end is
    // the base plus the size times 0x4000 in BYTES, computed in 32 bits (slwi 14), so a size of 0x40000 or more wraps on the Mac.
    // The parameter names are invented.
    static void RegisterMapGrid(const CMapCell* grid, int32_t cellSize);
    // 0x100f47e0 (36 bytes): the same for the track grid.
    static void RegisterTrackGrid(const CTrackCell* grid, int32_t cellSize);
    // Unit w146 (src/game/park/CellBase.cpp, unit w179, 0x100f49fc, 836 bytes): the 0x24 bytes both cells share; the serialisers of the cells ignore its answer.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);

    // Unit w139 (src/game/park/CellBase.cpp, unit w179): the map cell a cell stands for. 0x100f4928 (68 bytes, the CONST one: asks CWorldSavvy::GetMap on
    // the cell) and 0x100f49a0 (44 bytes, reads gWorldMap). A cell with the id 0 lands one cell before the grid.
    const CMapCell* GetMapCellEquiv() const;
    CMapCell* GetMapCellEquiv();
    // Unit w144 (src/game/park/CellBase.cpp, unit w179): 0x100f48c4 (48 bytes), the track cell a cell stands for.
    CTrackCell* GetTrackCellEquiv();
    // Unit w141 (0x100816e8 and 0x10081930 translated in src/game/park/CellBase.cpp, unit w179; the asm keeps the low byte), 0x100f4844 (the CONST overload).
    uint8_t GetNeighbours() const;
    uint8_t GetDirection() const;
    const CTrackCell* GetTrackCellEquiv() const;

    // Unit w179 (src/game/park/CellBase.cpp). The neighbour byte the next four use is the hoarding mask (+0x22) when the cell system's flag byte
    // (gLLCC4818) is nonzero and mKind is 2, else mNeighbours (+0xc), as RemoveNeighbour.
    void ModifyNeighbours(uint8_t mask);        // 0x10081608 (60 bytes): ORs the mask in
    int32_t NumberOfNeighbours() const;         // 0x10081744 (152 bytes): the set bits of the byte
    int32_t NumberOfHVNeighbours() const;       // 0x10081890 (104 bytes): the set bits among 0x01, 0x04, 0x10, 0x40
    void RemoveDirection(uint8_t direction);    // 0x1008184c (16 bytes): clears the bits in the direction byte
    int32_t NumberOfDirections() const;         // 0x10081968 (60 bytes): the set bits among 0x01, 0x04, 0x10, 0x40 of the direction byte
    void DecOverlapCounter();                   // 0x100819d8 (16 bytes): the halfword minus one (wraps)
    void SetOverlapCounter(int32_t count);      // 0x10081a1c (12 bytes): stores the low halfword
    int16_t GetOverlapCounter() const;          // 0x10081a5c (8 bytes): the signed halfword

    uint16_t mCellId;               // 0x00  1 + x + (y << 7) once the cell has its place (the constructors leave 0x423f / 0)
    uint8_t mKind;                  // 0x02  1 a map cell, 2 a track cell (set by the constructors)
    uint8_t mF03;                   // 0x03
    uint32_t mMeshInstance;         // 0x04  (ulong)
    CELL_MODES mType;               // 0x08
    uint8_t mNeighbours;            // 0x0c
    uint8_t mDirection;             // 0x0d
    uint16_t mFlags;                // 0x0e
    uint16_t mParentID;             // 0x10
    uint16_t mF12;                  // 0x12
    uint32_t mTileData[3];          // 0x14  (CELL_IndexType, ends before 0x20)
    int16_t mOverlapCounter;        // 0x20
    uint8_t mHoardingNeighbours;    // 0x22
    uint8_t mF23;                   // 0x23

    // Unit w120: a tile as the callers pass it, by value in r5 to r7 (12 bytes; mTileData has the same three words). The names are inferred
    // from CMapCell::SetHFieldTile (tile >> 16, tile & 0xffff, the tile's rotation); LLCCM_Create*Mesh never read mKind.
    struct CELL_IndexType {
        uint32_t mKind;       // word 0: 2 for a queue tile
        uint32_t mIndex;      // word 1: the index into the mesh table
        uint32_t mRotation;   // word 2: degrees
    };
};
static_assert(sizeof(CCellBase) == 0x24, "the shared part of a cell is 0x24 bytes");
static_assert(offsetof(CCellBase, mMeshInstance) == 4, "mMeshInstance at 4");
static_assert(offsetof(CCellBase, mType) == 8, "mType at 8");
static_assert(offsetof(CCellBase, mNeighbours) == 0xc, "mNeighbours at 0xc");
static_assert(offsetof(CCellBase, mFlags) == 0xe, "mFlags at 0xe");
static_assert(offsetof(CCellBase, mParentID) == 0x10, "mParentID at 0x10");
static_assert(offsetof(CCellBase, mTileData) == 0x14, "mTileData at 0x14");
static_assert(offsetof(CCellBase, mOverlapCounter) == 0x20, "mOverlapCounter at 0x20");
static_assert(offsetof(CCellBase, mHoardingNeighbours) == 0x22, "mHoardingNeighbours at 0x22");

// The cell classes derive from CCellBase, so their headers come first; they are included here for the users of the grids below.
#include "MapCell.h"
#include "TrackCell.h"

// Write-only registries in the shipped binary (nothing else reads them; one TOC slot each).
extern const CMapCell* gMapGridBase;      // 0x1034c1d0
extern const CMapCell* gMapGridEnd;       // 0x1034c1cc  base + size * 0x4000 bytes
extern int32_t gMapGridCellSize;          // 0x1034c1c0
extern const CTrackCell* gTrackGridBase;  // 0x1034c1c8
extern const CTrackCell* gTrackGridEnd;   // 0x1034c1c4
extern int32_t gTrackGridCellSize;        // 0x1034c1bc
