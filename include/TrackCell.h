// CTrackCell (the track cell, 0x28 bytes; the inventory files its code under TrainingCentre.cpp, unreliable; main app). PARTIAL: the
// constructor (unit w113) and Reset (unit w112c). The first 0x24 bytes are CCellBase's part (CellBase.h, with the names of the
// serialisers); +0x24 is mSegmentNumber (short, CTrackCell::Serialise 0x1012fbc0).
#pragma once
#include <cstdint>

#include "CellBase.h"

struct MapDelta;
struct IDXTYPEUniqueNameMapId;
template<class Tag, class T> struct tag_IDX_TYPE;

// Unit v5192 (src/game/park/TrackCell.cpp): globals CTrackCell::CalculateNeighbours reads (defined in that file).
extern int32_t gTrackNbrOn485C;      // 0x102e485c  nonzero: the last call's offsets (gLLCCE288, gLLCCE28C) replace the arguments
extern uint16_t gTrackNbr290;        // 0x1029e290  parent ids that join a track cell to its neighbour, per direction
extern uint16_t gTrackNbr292;        // 0x1029e292
extern uint16_t gTrackNbr294;        // 0x1029e294
extern uint16_t gTrackNbr296;        // 0x1029e296
extern uint16_t gTrackNbr298;        // 0x1029e298  the same four for the hoarding cells
extern uint16_t gTrackNbr29A;        // 0x1029e29a
extern uint16_t gTrackNbr29C;        // 0x1029e29c
extern uint16_t gTrackNbr29E;        // 0x1029e29e

class CTrackCell : public CCellBase {
public:
    CTrackCell();    // 0x1012fb08 (68 bytes, src/game/buildings/TrainingCentre.cpp)
    // Unit w144 (untranslated predicate): IsBridge 0x100a34c4 (IsHoarding and IsTrackX are unit w201's, below).
    int32_t IsBridge() const;   // Unit w141 (untranslated): 0x100a34c4

    // 0x100a3a74 (272 bytes, src/game/park/TrackCell.cpp): the cell's mode from the height field (HFieldCellStatus: 0 inside it, 7 outside via
    // ModifyType), then everything else back to its start state. Leaves the cell system's flag byte (gLLCC4818) 0.
    void Reset();
    // Untranslated callee of Reset (a stand-in in the launcher).
    void ModifyType(CELL_MODES mode);   // 0x100a0828 (776 bytes, src/game/park/TrackCell.cpp, unit w139)
    // Unit v5082 (untranslated callees of LLCC_BuildHoardingDB, stand-ins in the launcher).
    void Modify(CELL_MODES mode, int32_t a, int32_t b);   // 0x1009dfe4 (10,252 bytes)
    void ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id);   // 0x100a0b68 (16 bytes)
    // Unit w139: untranslated callees of ModifyType (stand-ins in the launcher).
    int32_t ValidTypeChange(CELL_MODES mode);   // 0x100a149c
    int32_t IsJustTrack() const;                // 0x100a3628
    // Unit w204 (src/game/park/TrackCell.cpp).
    uint8_t IsCrossRoad() const;   // 0x100a3914 (88 bytes): the neighbour byte when the cell has more than two horizontal / vertical neighbours, else 0
    uint8_t IsStraight() const;    // 0x100a3868 (124 bytes): the neighbour byte when exactly two, in a line, else 0
    // Unit w145 (untranslated callees of CMap::DeleteMapInEngine and UpdateMapInEngine, stand-ins in the launcher).
    void RemoveInstanceMesh();    // 0x100a4048 (68 bytes)
    void RefreshHFieldTile();     // 0x100a3ec0 (340 bytes)
    // Unit w146 (src/game/buildings/TrainingCentre.cpp, unit w179): 0x1012fbc0 (196 bytes), the cell's data after the shared part; 0x1012fb74 (8 bytes), whether the cell differs from a fresh one.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
    int32_t HasBeenModifiedFrom(const CTrackCell& other);   // (the mangled name has no const)

    // Unit w179 (src/game/buildings/TrainingCentre.cpp). 0x1012fcb8 (228 bytes): the track cell at the offset `delta` times `offset` away (NULL outside the level); unlike
    // CMapCell's twin the cell lies at the SCALED offset too. 0x1012fddc (412 bytes): the same for a compass direction bit (1, 2, 4 ... 0x80; any other
    // byte asserts and answers NULL).
    CTrackCell* GetNeighbouringCell(MapDelta delta, int32_t offset);
    CTrackCell* GetNeighbouringCell(uint8_t direction, int32_t offset);

    // Unit w185 (untranslated; LLCC_FixFirstBlock's calls; IsCorner is unit w201's, below).
    int32_t IsTrack() const;            // 0x100a3574 (60 bytes)
    int32_t IsTrackStartBlock() const;  // 0x100a36cc (156 bytes)
    void SetHFieldTile();               // 0x100a3bac (740 bytes)

    // Unit w201 (src/game/park/TrackCell.cpp): the flag word and the kind tests.
    uint16_t GetFlag();                 // 0x100a2998 (8 bytes): mFlags
    void SetFlag(uint16_t flags);       // 0x100a2964 (8 bytes): mFlags = flags (replaces, does not OR)
    void UnSetFlag(uint16_t flags);     // 0x100a29cc (16 bytes): clears the bits of flags in mFlags
    int32_t IsHoarding() const;         // 0x100a35dc (28 bytes): mType is 0x19
    int32_t IsTrackX() const;           // 0x100a367c (36 bytes): mType is 0xc or 0x11
    uint8_t IsCorner() const;           // 0x100a37a0 (156 bytes): the neighbour byte when exactly two horizontal / vertical neighbours at a right angle, else 0

    // Unit w206 (src/game/park/TrackCell.cpp; Delete and QuickDelete are untranslated stand-ins, the Mac ignores r3 in both).
    int32_t IsFlag(uint16_t flag) const;   // 0x100a2918 (32 bytes): whether any bit of flag is set in mFlags
    void Delete(int32_t a, int32_t b, bool c);   // 0x100a40c4 (3,480 bytes, untranslated; the answer type is a doubt)
    void QuickDelete();                          // 0x100a4e88 (480 bytes, src/game/park/TrackCell.cpp)
    // Unit v5005: 0x100a2a0c (1728 bytes, untranslated): the tile word (kind << 16 | index) and rotation of the cell's mode.
    uint32_t GetTile(int32_t* rotation) const;

    // Unit v5182 (untranslated stand-ins): 0x100a399c (104 bytes), the segment number (a halfword the callers extend); 0x100a1594 (4,932 bytes).
    int16_t GetSegmentNumber() const;
    int32_t ConnectionType(CELL_MODES mode, int32_t a, uint8_t b);

    int16_t mSegmentNumber;         // 0x24
    uint16_t mF26;                  // 0x26

    // Unit v5192: 0x100a0bd4 (1,732 bytes, src/game/park/TrackCell.cpp): joins the cell to the track or hoarding cells next to it.
    void CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b);

    // Unit v5635 (LLCR_PlaceRide's callee, stand-in): 0x100a5098 (68 bytes).
    void DeleteMesh();

    // Unit v5881 (src/game/park/TrackCell.cpp): Modify's real body, which answers r3 (0 or 1); the answer of ModifyType
    // (0x100a0828, r3; a weak stand-in in that file until ModifyType is translated with it); whether modes 0x81 and 0x83 leave the cell
    // alone (inlined in the original); CalculateDirections 0x100a12dc (392 bytes, untranslated stand-in).
    int32_t ModifyBody(CELL_MODES mode, int32_t a, int32_t b);
    int32_t ModifyTypeBody(CELL_MODES mode);
    bool ModifyLocked();

    // Unit v5696 (src/game/park/TrackCell.cpp): 0x100a12dc (392 bytes), adds the direction bits for the two steps (-2 or 2 each).
    void CalculateDirections(int32_t a, int32_t b);

    // Unit v5982 (LLCC_BuildTrackDB's callee, stand-in): 0x100a3a38 (8 bytes), stores the halfword at +0x24.
    void SetSegmentNumber(int16_t number);
};
static_assert(sizeof(CTrackCell) == 0x28, "a track cell is 0x28 bytes");

// Unit w139 (ModifyType). Globals the launcher zero-fills: the free-track flag (a byte) and the cell system's flag word gLLCC4858 (by rights
// LLCellSystem.h's, unit w133).
extern char gTrackCostFree;      // 0x102ddb35
extern int32_t gLLCC4858;        // 0x102e4858
uint32_t LLCC_SystemGetKartTrackCost();    // 0x10091c54 (src/game/park/LLCellSystem.cpp, unit w185)
uint32_t LLCC_SystemGetWaterTrackCost();   // 0x10091cd4 (src/game/park/LLCellSystem.cpp, unit w185)

// Unit v5005 (src/game/park/TrackCell.cpp): the scratch tile (tile >> 16, tile & 0xffff, rotation) SetHFieldTile
// builds before copying it to +0x14, and the mesh destroyers it calls (stand-ins in the launcher).
extern int32_t gLLCC4840[3];                      // 0x102e4840
void LLCCM_DestroyKTrackMesh(uint32_t mesh);      // 0x10082120 (36 bytes)
void LLCCM_DestroyWTrackMesh(uint32_t mesh);      // 0x10082508 (36 bytes)
void LLCCM_DestroyHoardingMesh(uint32_t mesh);    // 0x1008288c (36 bytes)

// Unit v5629 (src/game/park/TrackCell.cpp, which defines them): cell system words only CTrackCell::ConnectionType
// names so far (names are the addresses).
extern int32_t gLLCCC20C;   // 0x102dc20c  set when a cell of the current segment joins (1 or 3 neighbours), cleared by ConnectionType
extern int32_t gLLCCDB40;   // 0x102ddb40  nonzero: a corner does not forbid the placement
extern int32_t gLLCCDB60;   // 0x102ddb60  read with gTrackNbrOn485C
extern int32_t gLLCCE0B4;   // 0x102de0b4  the overwrite priority (ride record word 0x54) of the last ride met
extern int32_t gLLCCE0B8;   // 0x102de0b8  the priority a ride must reach to be built over

// Unit v5881 (src/game/park/TrackCell.cpp, which defines them): globals only CTrackCell::Modify names so far.
extern int32_t gLLCCDC21C;         // 0x102dc21c  the last mode a type change accepted
extern int32_t gLLCC4854;          // 0x102e4854  set before a type change of a track or hoarding cell
extern uint16_t gTrackModifyNoParent;   // 0x1029e284  the parent id of a cell with no ride
extern uint16_t gTrackModifyNoThing;    // 0x1029e286  the thing id of a map cell with no object
