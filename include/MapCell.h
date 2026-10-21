// CMapCell (the map cell, 0x44 bytes; the inventory files its code under MapDelta.cpp, unreliable; main app). PARTIAL: the
// constructor (unit w113) and Reset (unit w112c). The first 0x24 bytes are CCellBase's part (CellBase.h, with the names of the
// serialisers); the rest, from the serialiser CMapCell::Serialise 0x10102a10:
//   +0x24 mWho (ushort), +0x26 mStatusFlags (uchar: the attribute byte of the level's attribute map, Base.map), +0x28 mLitterScript
//   (long), +0x2c mLitter (LitterType), +0x30 mLitterCollector (ushort), +0x34 mTimeMarkedForLitterCollection (uint), +0x38 a long,
//   +0x40 mPylonIndex (short).
#pragma once
#include <cstddef>
#include <cstdint>

#include "CellBase.h"

struct IDXTYPEUniqueNameThingId;
struct IDXTYPEUniqueNameMapId;
struct MapDelta;
class CTrackCell;
template<class Tag, class T> struct tag_IDX_TYPE;

class CMapCell : public CCellBase {
public:
    CMapCell();    // 0x10102948 (92 bytes, src/game/park/MapDelta.cpp)

    // 0x1009cb48 (580 bytes, src/game/park/MapCell.cpp): puts the cell back to its start state from the height field (HFieldCellStatus) and
    // its attribute byte; `full` also lets the tile be set (SetHFieldTile) and is handed on to CalculateNeighbours as it is.
    void Reset(bool full);
    // Untranslated callees of Reset (stand-ins in the launcher).
    void CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag);   // 0x10099998 (5,540 bytes)
    void SetHFieldTile();                                                            // 0x1009cdb4 (384 bytes, src/game/park/MapCell.cpp, unit w139)
    uint32_t GetTile(int32_t* rotation);   // 0x1009bc54 (untranslated, unit w139): the tile word (kind << 16 | index) and the rotation of the cell's mode
    // 0x10102d34 (200 bytes, untranslated; unit w122a's CThing::CThing calls it): puts the thing at the head of the cell's thing list (mWho
    // +0x24): the previous head's +8 and the new thing's +0xa are set, the new thing's +8 is cleared, mWho becomes the new id.
    void InsertThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);

    // Unit w139 (untranslated callees of CTrackCell::ModifyType): 0x101036b0, the neighbour at the offset (NULL for none); 0x1009d0f4, deletes.
    // 0x101036b0 (228 bytes, src/game/park/MapDelta.cpp, unit w145): the neighbour at the offset `delta` times flag away (NULL outside the level).
    // The cell it answers lies at the UNSCALED offset (the original's quirk, flag 1 in every caller of this file).
    CMapCell* GetNeighbouringCell(const MapDelta* delta, int32_t flag);
    void Delete(int32_t a, int32_t b);
    // Unit v5211 (src/game/park/MapCell.cpp): Delete's callee, 0x10099614 (740 bytes, stand-in): sets the cell's mode.
    // Unit v5535 (src/game/park/MapCell.cpp): the answer (r3) is 0 when the change is refused (or unaffordable), else 1.
    void ModifyType(CELL_MODES mode);
    int32_t ModifyTypeBody(CELL_MODES mode);   // the real body, answers r3 (0 refused, else 1)
    // Unit v5535: 0x1009c8c4 (124 bytes, stand-in): the neighbour byte when exactly two in a line, else 0 (the low byte of r3 is tested).
    uint8_t IsStraight() const;

    // Unit w204 (src/game/park/MapCell.cpp).
    uint8_t IsCorner() const;   // 0x1009c7fc (156 bytes): the neighbour byte when exactly two horizontal / vertical neighbours at a right angle, else 0

    // Unit w145 (src/game/park/MapDelta.cpp).
    int32_t HasBeenModifiedFrom(const CMapCell& other) const;   // 0x101029c8 (8 bytes): 1
    // 0x10102e54 (356 bytes): takes the thing out of the cell's list of things (mWho, linked by CThing::mCellPrev and mCellNext).
    void RemoveThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);
    void MarkForLitterCollection(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> collector);   // 0x101033a0 (36 bytes)
    int32_t MarkedForLitterCollection();   // 0x10103428 (220 bytes): 1 while the mark is younger than 100 turns; an old one is cleared
    void RemoveLitter();                   // 0x10103540 (324 bytes)
    CMapCell* GetNeighbouringCell(uint8_t direction);   // 0x10103814 (444 bytes): the cell in a compass direction bit (1, 2, 4 ... 0x80)
    void RemoveInstanceMesh();    // 0x1009d07c (68 bytes, untranslated)
    void RefreshHFieldTile();     // 0x1009cf64 (228 bytes, untranslated)

    // Unit w146 (src/game/park/MapDelta.cpp). The kind of litter (values 0 to 8 are used; 7 and 8 have a region effector; no names are known).
    enum LitterType : int32_t {};
    // 0x10103010 (844 bytes): puts litter of the kind on the cell (nothing in an online game or when the cell has litter already): a sprite script
    // at a random spot in the cell, and for kind 8 a particle emitter.
    void PlaceLitter(LitterType type);
    // 0x10102a10 (756 bytes): CCellBase::Serialise first (its answer is ignored), then, for a version 0 or 1 only, the eight values; 0 on success, 1 at
    // the first value that fails. `save` 0 loads. A version out of range does nothing and answers 0.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);

    // Unit w142 (untranslated predicates and a hidden-result getter ContextMouse asks).
    int32_t IsEntrance() const;   // 0x1009c4dc
    int32_t IsExit() const;       // 0x1009c524
    int32_t IsRide() const;       // 0x1009c61c
    int32_t IsGrass() const;      // 0x1009c660
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetObjectThingId() const;   // 0x10103a04 (hidden halfword result)
    // Unit w201 (src/game/park/MapCell.cpp):
    int32_t IsFlag(uint16_t flag) const;   // 0x1009bb70 (32 bytes): whether any bit of flag is set in mFlags
    // Unit w206 (src/game/park/MapCell.cpp):
    void UnSetFlag(uint16_t flag);         // 0x1009bc18 (16 bytes): clears the bits of flag in mFlags
    int32_t IsWater() const;               // 0x1009c408 (28 bytes): mType is 2
    int32_t IsFreeform() const;            // 0x1009c59c (36 bytes): mType is 7 or 0x1e
    // Unit w143 (untranslated predicates, stand-ins in the launcher; each answers an int32).
    int32_t IsPath() const;                // 0x1009c44c
    int32_t IsQueue() const;               // 0x1009c490
    int32_t IsTrack() const;               // 0x1009c96c (176 bytes)
    int32_t IsHoarding() const;            // 0x1009ca9c (128 bytes)
    // Unit w144 (untranslated): 0x1009c6ac (28 bytes).
    int32_t IsWalkable() const;
    // Unit w141 (untranslated predicates MAP_wall_in_way asks; the asm tests r3 against 0).
    int32_t IsWalkableInFirstPerson() const;  // 0x1009c6f4
    int32_t IsNoGo() const;                   // 0x1009c75c
    int32_t IsTrackX() const;                 // 0x1009ca44
    // Unit w156 (untranslated predicates the park analyser's map loops ask).
    int32_t IsBridge() const;                 // 0x1009c568 (8 bytes)
    int32_t IsLake() const;                   // 0x1009c5ec
    int32_t IsCoasterPylon() const;           // 0x1009c7b0
    // Unit v5270 (src/game/park/MapCell.cpp; IsTrack, IsHoarding and RefreshHFieldTile are declared above).
    int32_t ValidTypeChange(CELL_MODES mode);             // 0x1009b054 (200 bytes)
    void CalculateDirections(int32_t a, int32_t b);       // 0x1009af80 (160 bytes)
    // Unit v5536 (LLCC_DrawKartBlock's callee, 0x10097538, 8360 bytes, stand-in): sets the cell's mode; answers 0 on failure.
    int32_t Modify(CELL_MODES mode, int32_t a, int32_t b);
    // Unit v5633 (LLCM_MouseMoved's callee, 0x1009d860, 8 bytes, stand-in): the pylon index halfword (the caller sign-extends it).
    int16_t GetPylonIndex() const;
    // Unit v5637 (src/game/park/MapCell.cpp): Modify's callees (stand-ins).
    int32_t ConnectionType(CELL_MODES mode, int32_t flag);   // 0x1009b158 (2,524 bytes): 1 for an illegal placement
    void SetPylonIndex(int16_t index);   // 0x1009d898 (8 bytes)

    uint16_t mWho;                 // 0x24
    uint8_t mStatusFlags;           // 0x26  the attribute byte (bits 1, 2, 4, 8, 0x10 and 0x80 are read by Reset)
    uint8_t mF27;                   // 0x27
    int32_t mLitterScript;          // 0x28  the script handle of the litter (SCR_KillScript); 0 for none
    LitterType mLitter;             // 0x2c  the kind of litter; 0 for none, 7 and 8 have a region effector
    uint16_t mLitterCollector;      // 0x30  the person marked to collect it; the sentinel gLitterNoCollector for none
    uint16_t mF32;                  // 0x32
    uint32_t mTimeMarkedForLitterCollection;   // 0x34  gWorld->mTurn when it was marked
    int32_t mLitterEmitter;         // 0x38  the particle emitter handle of kind 8 litter (the name is invented)
    uint32_t mF3C;                  // 0x3c  cleared before the memset (redundant); the patrol mark (unit w156): ClearMapData clears it, MarkAreaPatrolledBy sets 1
    int16_t mPylonIndex;            // 0x40  zero (the serialiser's name)
    uint16_t mF42;                  // 0x42  zero

    // Unit v5635 (LLCR_PlaceRide's callees, stand-ins): 0x1009bbb8 (8 bytes) the flag word; 0x10099930 (16 bytes) the parent cell id
    // (by value, the HIGH halfword of the Mac's register).
    void SetFlag(uint16_t flag);
    void ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id);
    // Unit v5881 (CTrackCell::Modify's callee, stand-in): 0x1009bbe8 (8 bytes), the flag word.
    uint16_t GetFlag();
};
static_assert(sizeof(CMapCell) == 0x44, "a map cell is 0x44 bytes");
static_assert(offsetof(CMapCell, mWho) == 0x24, "mWho at 0x24");
static_assert(offsetof(CMapCell, mLitterScript) == 0x28, "mLitterScript at 0x28");
static_assert(offsetof(CMapCell, mLitter) == 0x2c, "mLitter at 0x2c");
static_assert(offsetof(CMapCell, mLitterCollector) == 0x30, "mLitterCollector at 0x30");
static_assert(offsetof(CMapCell, mTimeMarkedForLitterCollection) == 0x34, "mTimeMarkedForLitterCollection at 0x34");
static_assert(offsetof(CMapCell, mLitterEmitter) == 0x38, "mLitterEmitter at 0x38");
static_assert(offsetof(CMapCell, mF3C) == 0x3c, "mF3C at 0x3c");
static_assert(offsetof(CMapCell, mPylonIndex) == 0x40, "mPylonIndex at 0x40");

// Unit w123a: the unsigned cell-head sentinel (initial value 0), Mac 0x102a0b24.
extern uint16_t gThingLoopEmptyCellId;

// Unit w139 (src/game/park/MapCell.cpp: CMapCell::SetHFieldTile 0x1009cdb4). Untranslated callees (stand-ins in the launcher), globals the launcher
// zero-fills.
extern CCellBase::CELL_IndexType gLLCC4824;   // 0x102e4824, the tile the cell system hands on
void SingleHFieldTextureChange(int32_t x, int32_t y, const CCellBase::CELL_IndexType* tile);   // 0x1005b840
void LLCCM_DestroyQMesh(uint32_t mesh);   // 0x10081d80

// Unit w145 (src/game/park/MapDelta.cpp, src/game/park/Map.cpp): halfwords the original compares thing ids and cell ids with, all 0 and never written (Mac 0x102a0b26,
// 0x102a0b28, 0x102a0b2a, 0x102a0b50, 0x102a0b52 and 0x102a0a9e; the names are invented). RemoveThing compares the thing's previous link,
// the cell's new head and the thing's next link with the first three; MarkedForLitterCollection the collector with the fourth;
// GetObjectThingId the walk's thing with the fifth; CMap::SetTimeStamp the cell id with the sixth.
extern uint16_t gRemoveThingNoPrev;       // 0x102a0b26
extern uint16_t gRemoveThingNoWho;        // 0x102a0b28
extern uint16_t gRemoveThingNoNext;       // 0x102a0b2a
extern uint16_t gLitterNoCollector;       // 0x102a0b50
extern uint16_t gGetObjectThingIdEnd;     // 0x102a0b52
extern uint16_t gMapEmptyCellId;          // 0x102a0a9e

// Unit v5535 (src/game/park/MapCell.cpp; names invented). GetTile's two tables of 12 byte entries (tile word kind << 16 | index,
// rotation, neighbour mask byte, three pad bytes); the initial data is the Mac's (unit v5L35).
struct CMapCellTileEntry {
    uint32_t mTile;       // 0x00
    int32_t mRotation;    // 0x04
    uint8_t mMask;        // 0x08
    uint8_t mPad[3];      // 0x09
};
extern CMapCellTileEntry gTileTableA[11];   // 0x1029d870 (for the mode 3 cells), count gTileCountA
extern int32_t gTileCountA;                 // 0x1029d8f4 (11)
extern CMapCellTileEntry gTileTableB[49];   // 0x1029d8f8 (every other mode), count gTileCountB
extern int32_t gTileCountB;                 // 0x1029db44 (0x31)
extern uint8_t gTileRandomFlag;             // 0x102e4830 rand() % 2 of the last GetTile
extern uint16_t gNoObjectThingId;           // 0x1029e212 halfword, 0: ModifyType compares the queue's object thing id with it
extern uint16_t gEntranceWalkEnd;        // 0x102a0af0  0: the end of the thing list MAP_wall_in_way walks (name invented; src/game/park/Map.cpp)

// Unit v5070 (src/game/park/TrackCell.cpp). 0x1005f160 (620 bytes, stand-in): the ground height of a square (the third argument is not read here).
float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra);
// 0x1009d8d0 (1756 bytes): the height of the cell's ground rounded up to a multiple of 10 for the cell and its +Y, +X and +X+Y neighbours (the last one found wins); `height` is kept when none counts.
float GetTrackMapCellHeight(CMapCell* cell, float height, int32_t x, int32_t y);

// Unit v5211 (src/game/park/MapCell.cpp, CMapCell::Delete): halfwords it compares ids with, both 0 and never written (names invented).
extern uint16_t gDeleteNoParent;   // 0x1029e226
extern uint16_t gDeleteNoThing;    // 0x1029e228

// Unit v5270 (src/game/park/MapCell.cpp, defined there; names invented, both start at 0).
extern uint16_t gIsTrackNoParent;      // 0x1029e224
extern int32_t gValidTypeChangeLock;   // 0x102e4834

// Unit v5632 (src/game/park/MapCell.cpp, defined there; names invented). CalculateNeighbours keeps its last two
// direction arguments (initially 0x80, the "none" value) and compares the parent ids of the neighbours with four halfwords (all 0).
extern int32_t gCalcNeighboursPrevA;        // 0x1029e214  the last `a` (0x80 at start)
extern int32_t gCalcNeighboursPrevB;        // 0x1029e218  the last `b` (0x80 at start)
extern uint16_t gCalcNeighboursIdMinusY;    // 0x1029e21c  0
extern uint16_t gCalcNeighboursIdPlusX;     // 0x1029e21e  0
extern uint16_t gCalcNeighboursIdPlusY;     // 0x1029e220  0
extern uint16_t gCalcNeighboursIdMinusX;    // 0x1029e222  0
extern int32_t gCalcNeighboursJoinEntry;    // 0x102e4838  0 (BSS): nonzero joins the current object's entry cell and uses the kept a / b

// Unit v5637 (src/game/park/MapCell.cpp, defined there; names invented, all 0 at start).
extern int32_t gModifyLastMode;      // 0x102dc21c  the last mode ModifyType accepted (0x80 recalculates the neighbours with it)
extern int32_t gModifyTypeRefused;   // 0x102e483c  set when the change is refused under the lock (0x81 answers 0 then)
extern int32_t gModifyPylonIndex;    // 0x102ddb70  the next pylon index (0x84)
extern uint16_t gModifyNoParent;     // 0x1029e20e  0
extern uint16_t gModifyNoThing;      // 0x1029e210  0
void RemoveBridge(CTrackCell* cell, MapDelta a, MapDelta b);   // 0x1009722c (712 bytes, stand-in)
