// TrackCell.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

// LLCellSystem.h declares LLCC_DrawTrackLine void, but the listing answers r3 and this file defines it so (the same
// mangled name): the header leaves its declaration out here.
#define TPW_LLCC_DRAWTRACKLINE_R3
#include "ActionRecorder.h"
#include "analyser.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "ui_publishParkPanel.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

// GetTrackMapCellHeight 0x1009d8d0 (1756 bytes): the height a track piece takes from the map cell and its three neighbours.

namespace {

// The cell types whose ground height counts: 4, 7, 9, 10, 0xb, 0x10 and 0x19 (the sets are disjoint). The height of the
// square is truncated, plus 9, divided by 10 (truncating) and scaled by 10.0f: a round up to the next multiple of 10.
bool UsesGround(int32_t type) {
    return type == 0xb || type == 0x10 || type == 0x19 || type == 7 || type == 4 || static_cast<uint32_t>(type - 9) < 2u;
}

float RoundedHeight(int32_t x, int32_t y) {
    const float height = GetWorldGSquareHeight(x, y, nullptr);
    const int32_t n = static_cast<int32_t>(height) + 9;
    const int32_t q = n / 10;
    return 10.0f * static_cast<float>(q);
}

float Neighbour(CMapCell* cell, const MapDelta& delta, float current) {
    MapDelta d;
    d.mX = delta.mX;
    d.mY = delta.mY;
    d.mFlag = 0;
    CMapCell* other = cell->GetNeighbouringCell(&d, 1);
    if (other == nullptr) return current;
    const uint32_t id = other->mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    if (UsesGround(static_cast<int32_t>(other->mType))) return RoundedHeight(x, y);
    return current;
}

}  // namespace

float GetTrackMapCellHeight(CMapCell* cell, float height, int32_t x, int32_t y) {
    if (UsesGround(static_cast<int32_t>(cell->mType))) height = RoundedHeight(x, y);
    height = Neighbour(cell, gMapDeltaPlusY, height);
    height = Neighbour(cell, gMapDeltaPlusX, height);
    height = Neighbour(cell, gMapDeltaPlusXPlusY, height);
    return height;
}

// LLCellSystem.h declares this void, but the listing answers r3 (1, or 0 when a cell refused); the same mangled name, so it is
// declared here with its real return type.
int32_t LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);

namespace {

// 0x1009dfe4 and CMap::IsXYValid are the callees; the cell is the track cell of the map cell (x + (y << 7) + 1), 16 bits.
CTrackCell* CellAt(int32_t x, int32_t y) {
    const uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];
}

// Rounds the distance down to an even number (the listing's srawi/addze/slwi), returns the step towards the end.
int32_t Snap(int32_t from, int32_t* end) {
    int32_t d = from - *end;
    if (d > 0 && d != (d / 2) * 2) d = d + 1;
    *end = from - (d / 2) * 2;
    if (from > *end) return -2;
    if (from < *end) return 2;
    return 0;
}

}  // namespace

// CTrackCell::Modify (0x1009dfe4, 10,252 bytes, unit v5881): the track cell system's one entry for a change to a track or hoarding cell.
// Bit 0x100 of the mode draws the cell's blueprint (a square face at the cell's height plus wall faces down to every lower neighbour);
// otherwise the low byte picks the action: 0x32 deletes, 0x80 joins the neighbours, 0x81 fixes the flags and tiles of the hoarding,
// 0x82 the directions, 0x83 hands the cell's parent id to three neighbours, 0x85 fixes the flags against the water around, 0x86 fixes
// the bridges of the ride on the cell, 0x87 deletes the rides under the cell, and any other mode changes the cell's type.
// The Mac answers r3 (ModifyBody below); the declared Modify is void and passes the call on.

// CMapCell::Modify's unit already owns gModifyNoParent / gModifyNoThing (other addresses): these two get their own symbols.
#define gModifyNoParent gTrackModifyNoParent
#define gModifyNoThing gTrackModifyNoThing

// Globals only this function names so far (names invented; the launcher zero-fills them like the Mac's bss).
int32_t gLLCCDC21C;        // 0x102dc21c  the last mode a type change accepted (CalculateNeighbours's mode)
int32_t gLLCC4854;         // 0x102e4854  set before a type change of a track or hoarding cell, cleared by mode 0x81
uint16_t gModifyNoParent;  // 0x1029e284  the parent id of a cell with no ride (mode 0x86)
uint16_t gModifyNoThing;   // 0x1029e286  the thing id of a map cell with no object (mode 0x86)

namespace {

// The ground height of a square rounded up to a step of 10 (fctiwz, then (h + 9) / 10 truncated, times 10.0f).
float GroundStep(uint32_t x, uint32_t y) {
    const float ground = GetWorldGSquareHeight(static_cast<int32_t>(x), static_cast<int32_t>(y), nullptr);
    const int32_t steps = (ppc_fctiwz(ground) + 9) / 10;
    return 10.0f * static_cast<float>(steps);
}

// The height a cell of the type starts from before GetTrackMapCellHeight (0.0f for the types without ground); the three tests are
// separate in the original, so a type can never match two of them.
float BaseHeight(int32_t type, uint32_t x, uint32_t y) {
    float base = 0.0f;
    if (type == 0xb || type == 0x10 || type == 0x19) base = GroundStep(x, y);
    if (type == 7) base = GroundStep(x, y);
    if (type == 4 || static_cast<uint32_t>(type - 9) <= 1u) base = GroundStep(x, y);
    return base;
}

// The cell's height: x and y from the cell id (1 + x + (y << 7); an id of 0 gives x 0x7f and y 0x1ffffff, unsigned).
float CellHeight(CTrackCell* cell) {
    const uint32_t index = static_cast<uint32_t>(cell->mCellId) - 1u;
    const uint32_t x = index & 0x7fu;
    const uint32_t y = index >> 7;
    const float base = BaseHeight(static_cast<int32_t>(cell->mType), x, y);
    return GetTrackMapCellHeight(cell->GetMapCellEquiv(), base, static_cast<int32_t>(x), static_cast<int32_t>(y));
}

// The wall faces on one side of the blueprint: one per 10 units from the neighbour's height up to the cell's (none when the neighbour is
// missing or as high; the bounds are fctiwz of both heights).
void Walls(CTrackCell* self, const MapDelta& delta, float height, float x, float z, uint8_t side, int32_t texture, int32_t keep) {
    CTrackCell* const next = self->GetNeighbouringCell(delta, 2);
    if (next == nullptr) return;
    const float other = CellHeight(next);
    if (height == other) return;
    for (int32_t step = ppc_fctiwz(other); step < ppc_fctiwz(height); step += 10) {
        DC_AddSquareFaceToList(x, 1.5f + static_cast<float>(step), z, side, texture, 0, 0.0f, 1, 0, keep, static_cast<DYN_Type>(0));
    }
}

// The flag 0x40 of a map cell's neighbour: cleared, or set to the CENTRE cell's flags with 0x40 (the original reads `cell`, not the
// neighbour); the neighbour's tile follows either way.
void HoardingNeighbour(CMapCell* cell, const MapDelta& delta, bool clear) {
    MapDelta offset = delta;
    CMapCell* const next = cell->GetNeighbouringCell(&offset, 1);
    if (next == nullptr) return;
    if (clear) {
        next->UnSetFlag(0x40);
    } else {
        next->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x40));
    }
    next->SetHFieldTile();
}

}  // namespace

// Stand-in for the answer of CTrackCell::ModifyType (0x100a0828): the original returns r3 (read here), the translation in
// src/TrackCell_modify.cpp is void. Until ModifyType is translated again with its answer this guesses it from the type it leaves
// (weak: a translation that defines ModifyTypeBody replaces it; the test library defines a fake).
__attribute__((weak)) int32_t CTrackCell::ModifyTypeBody(CELL_MODES mode) {
    ModifyType(mode);
    return mType == mode ? 1 : 0;
}

// The cell types modes 0x81 and 0x83 leave alone while the cell system's flag byte is set (0xc is tested twice in the original).
bool CTrackCell::ModifyLocked() {
    if (gLLCC4818 == 0) return false;
    const int32_t type = static_cast<int32_t>(mType);
    if (type == 0xb || type == 0xc || type == 0x10 || type == 0xc || type == 0x11) return true;
    return static_cast<int32_t>(GetMapCellEquiv()->mType) == 0x15;
}

void CTrackCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    ModifyBody(mode, a, b);
}

int32_t CTrackCell::ModifyBody(CELL_MODES mode, int32_t a, int32_t b) {
    const int32_t full = static_cast<int32_t>(mode);
    const int32_t low = full & 0xff;

    if ((full & 0x100) != 0) {
        // The blueprint. The texture and rotation of the cell's face: the arguments for mode 4 and for modes 0xb and 0x10 with a == 5.
        const uint32_t index = static_cast<uint32_t>(mCellId) - 1u;
        const uint32_t x = index & 0x7fu;
        const uint32_t y = index >> 7;
        int32_t texture = 0;
        int32_t rotation = 1;
        if (low == 4 || (low == 0xb && a == 5) || (low == 0x10 && a == 5)) {
            texture = a;
            rotation = b;
        }
        const float height = CellHeight(this);
        const float fx = static_cast<float>(static_cast<int32_t>(x * 10u));
        const float fz = static_cast<float>(static_cast<int32_t>(y * 10u));
        uint8_t side = 0;
        if (a == -2) side = 0x40;
        if (a == 2) side = 4;
        if (b == -2) side = 1;
        if (b == 2) side = 0x10;

        const int32_t connection = ConnectionType(static_cast<CELL_MODES>(low), 0, side);
        if (connection == 1) {
            // An illegal placement: the second call's answer is not read.
            ConnectionType(static_cast<CELL_MODES>(low), 1, side);
            LLCM_SetIllegalPlacement(1);
            DC_AddSquareFaceToList(fx, 1.5f + height, fz, 0, 1, 1, 0.0f, 1, 0, 0, static_cast<DYN_Type>(0));
            Walls(this, gMapDeltaMinusY, height, fx, fz, 0x01, 1, 0);
            Walls(this, gMapDeltaPlusX, height, fx, fz, 0x04, 1, 0);
            Walls(this, gMapDeltaPlusY, height, fx, fz, 0x10, 1, 0);
            Walls(this, gMapDeltaMinusX, height, fx, fz, 0x40, 1, 0);
        } else {
            DC_AddSquareFaceToList(fx, 1.5f + height, fz, 0, connection != 0 ? connection : texture, rotation, 0.0f, 1, 0, 0,
                                   static_cast<DYN_Type>(0));
            const int32_t keep = connection == 0xd ? 1 : 0;
            Walls(this, gMapDeltaMinusY, height, fx, fz, 0x01, 0, keep);
            Walls(this, gMapDeltaPlusX, height, fx, fz, 0x04, 0, keep);
            Walls(this, gMapDeltaPlusY, height, fx, fz, 0x10, 0, keep);
            Walls(this, gMapDeltaMinusX, height, fx, fz, 0x40, 0, keep);
        }
        return 1;
    }

    switch (low) {
    case 0x32:
        Delete(a, b, true);
        return 1;

    case 0x80:
        CalculateNeighbours(static_cast<CELL_MODES>(gLLCCDC21C), a, b);
        return 1;

    case 0x81: {
        if (ModifyLocked()) return 1;
        if (static_cast<int32_t>(mType) == 0x19) {
            CMapCell* const cell = GetMapCellEquiv();
            bool self = false;
            bool plusX = false;
            bool plusY = false;
            bool plusXY = false;
            bool corner = false;
            const uint8_t direction = GetDirection();
            const uint8_t neighbours = GetNeighbours();
            if ((direction == 1 && neighbours == 0x41) || (direction == 0x10 && neighbours == 0x14) || (direction == 4 && neighbours == 5) ||
                (direction == 0x40 && neighbours == 0x50) || (direction == 0x40 && neighbours == 0x14) || (direction == 1 && neighbours == 0x50) ||
                (direction == 4 && neighbours == 0x41) || (direction == 0x10 && neighbours == 5)) {
                corner = true;
            }
            const uint32_t index = static_cast<uint32_t>(cell->mCellId) - 1u;
            const int32_t x = static_cast<int32_t>(index & 0x7fu);
            const int32_t y = static_cast<int32_t>(index >> 7);
            // The two ends of the hoarding count as straight pieces.
            const bool end = (x == gHoardingRight && y == gHoardingBottom) || (x == gHoardingLeft && y == gHoardingTop);
            switch (direction) {
            case 1:
                if (IsStraight() != 0 || end) {
                    plusX = plusXY = true;
                } else if (IsCorner() != 0) {
                    if (corner) self = plusX = plusY = plusXY = true;
                    else plusX = true;
                }
                break;
            case 4:
                if (IsStraight() != 0 || end) {
                    plusY = plusXY = true;
                } else if (IsCorner() != 0) {
                    if (corner) self = plusX = plusY = plusXY = true;
                    else plusXY = true;
                }
                break;
            case 0x10:
                if (IsStraight() != 0 || end) {
                    self = plusY = true;
                } else if (IsCorner() != 0) {
                    if (corner) self = plusX = plusY = plusXY = true;
                    else plusY = true;
                }
                break;
            case 0x40:
                if (IsStraight() != 0 || end) {
                    self = plusX = true;
                } else if (IsCorner() != 0) {
                    if (corner) self = plusX = plusY = plusXY = true;
                    else self = true;
                }
                break;
            default:
                break;
            }
            if (self) {
                cell->UnSetFlag(0x40);
            } else {
                cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x40));
            }
            cell->SetHFieldTile();
            HoardingNeighbour(cell, gMapDeltaPlusX, plusX);
            HoardingNeighbour(cell, gMapDeltaPlusY, plusY);
            HoardingNeighbour(cell, gMapDeltaPlusXPlusY, plusXY);
        } else if (IsCorner() != 0 || IsCrossRoad() != 0) {
            UnSetFlag(0xf);
        }
        gLLCCDC21C = 0;
        if (static_cast<int32_t>(mType) != 0xd && static_cast<int32_t>(mType) != 0x12) SetHFieldTile();
        if (gLLCC4858 != 0 && gLLCC4854 != 0) {
            gLLCC4854 = 0;
            return 0;
        }
        return 1;
    }

    case 0x82:
        CalculateDirections(a, b);
        return 1;

    case 0x83: {
        const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> self = {mCellId};
        if (ModifyLocked()) return 1;
        if (gLLCCDB48 != nullptr) {
            const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> parent = {gLLCCDB68};
            ModifyParentID(parent);
            const MapDelta* const deltas[3] = {&gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaPlusXPlusY};
            for (const MapDelta* delta : deltas) {
                CTrackCell* const next = GetNeighbouringCell(*delta, 1);
                if (next != nullptr) {
                    next->ModifyParentID(self);
                    next->ModifyType(static_cast<CELL_MODES>(0xc));
                }
            }
        }
        return 1;
    }

    case 0x85: {
        CMapCell* const cell = GetMapCellEquiv();
        if (IsCorner() != 0 || IsCrossRoad() != 0) return 1;
        const MapDelta* const deltas[3] = {&gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaPlusXPlusY};
        if (static_cast<int32_t>(mType) == 0x19) {
            if (cell->IsWater() != 0 || cell->IsFreeform() != 0) {
                UnSetFlag(0xf);
                SetFlag(1);
            }
            for (const MapDelta* delta : deltas) {
                MapDelta offset = *delta;
                CMapCell* const next = cell->GetNeighbouringCell(&offset, 1);
                if (next != nullptr && (next->IsWater() != 0 || next->IsFreeform() != 0)) {
                    UnSetFlag(0xf);
                    SetFlag(1);
                }
            }
        } else {
            int32_t empty = 0;
            int32_t type = static_cast<int32_t>(cell->mType);
            if (type == 1 || type == 3) {
                UnSetFlag(0xf);
                SetFlag(1);
            }
            if (type == 0) ++empty;
            for (const MapDelta* delta : deltas) {
                MapDelta offset = *delta;
                CMapCell* const next = cell->GetNeighbouringCell(&offset, 1);
                if (next == nullptr) continue;
                type = static_cast<int32_t>(next->mType);
                if (type == 1 || type == 3) {
                    UnSetFlag(0xf);
                    SetFlag(1);
                }
                if (type == 0) ++empty;
            }
            if (empty == 4) UnSetFlag(0xf);
        }
        return 1;
    }

    case 0x86: {
        if (IsTrack() == 0) return 1;
        if (IsHoarding() != 0) return 1;
        if (gLLCC4818 != 0) return 1;
        if (mParentID == gModifyNoParent) return 1;
        // The map cell of the parent id (id - 1, unchecked: an id of 0 lands one cell before the grid).
        const CMapCell* const parent = gGameWorld->mMapCells + (static_cast<int32_t>(mParentID) - 1);
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = parent->GetObjectThingId();
        if (thing.mIndex == gModifyNoThing) return 1;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        CObject* const ride = static_cast<CObject*>(gThingArray[thing.mIndex].mThing);
        if (LLCR_FixBridges(ride, true, false, false) == 0) LLCR_FixBridgesReverse(ride, false, false, false);
        return 1;
    }

    case 0x87: {
        if (gLLCCDE0B8 > gLLCCDE0B4) {
            const uint32_t index = static_cast<uint32_t>(mCellId) - 1u;
            const int32_t x = static_cast<int32_t>(index & 0x7fu);
            const int32_t y = static_cast<int32_t>(index >> 7);
            const int32_t saved4804 = gLLCC4804;
            const int32_t saved4800 = gLLCC4800;
            const int32_t systemMode = LLCC_SystemGetMode();
            if (IsTrack() == 0) {
                LLCR_DeleteRide(x, y, 0);
                LLCR_DeleteRide(x + 1, y, 0);
                LLCR_DeleteRide(x, y + 1, 0);
                LLCR_DeleteRide(x + 1, y + 1, 0);
                if (gLLCC4858 != 0) {
                    // The original deletes the three neighbours without testing them for NULL.
                    CMapCell* const cell = GetMapCellEquiv();
                    cell->Delete(0, 0);
                    MapDelta offset = gMapDeltaPlusX;
                    cell->GetNeighbouringCell(&offset, 1)->Delete(0, 0);
                    offset = gMapDeltaPlusY;
                    cell->GetNeighbouringCell(&offset, 1)->Delete(0, 0);
                    offset = gMapDeltaPlusXPlusY;
                    cell->GetNeighbouringCell(&offset, 1)->Delete(0, 0);
                }
            }
            gLLCC4804 = saved4804;
            gLLCC4800 = saved4800;
            LLCC_SystemSetModeNR(systemMode, false);
        }
        return 1;
    }

    default: {
        // A type change (the whole mode is passed on and stored).
        gLLCC4854 = 0;
        const int32_t type = static_cast<int32_t>(mType);
        if ((gLLCC4858 != 0 && (type == 0xd || type == 0x12 || type == 0xb || type == 0x10)) || static_cast<int32_t>(mType) == 0x19) {
            gLLCC4854 = 1;
        }
        if (ModifyTypeBody(mode) != 0) {
            gLLCCDC21C = full;
            return 1;
        }
        return full == 0x19 ? 1 : 0;
    }
    }
}

// CTrackCell::ModifyType (main app, SimTheme Park 2.04b3, TrackCell.cpp; unit w139), 0x100a0828, 776 bytes.
//
// Changes the mode of a track cell and pays for it. The cost is 0 except for modes 0xb (a kart track) and 0x10 (a water track) while
// gTrackCostFree is 0. Order: ValidTypeChange(mode) refuses (the Mac then answers whether the mode is 0x19, nothing else happens); a mode
// the cell already has only adds 1 to the overlap counter; otherwise, unless a recording runs, the bank balance must cover the cost
// ((balance - cost) >> 31, 32 bit) unless the mode is 0x19, 0 or 7, in which case nothing is stored when it does not. Then the mode is
// stored, the map stamps the cell with a new time stamp, and (for a plain track cell with gLLCC4858 set) the four map cells of the 2 x 2
// block the cell stands for are deleted when all four exist and their modes are 0 or 1. Last the cost is withdrawn (not for modes 0 and 7,
// nor when tracks are free). The Mac answers 1 (done / counted), 0 (refused or unaffordable) or (mode == 0x19) when the change is invalid;
// the declaration returns void and no caller reads it, so the answer is not reproduced.

void CTrackCell::ModifyType(CELL_MODES mode) {
    const int32_t m = static_cast<int32_t>(mode);
    uint32_t cost = 0;
    if (m == 0xb && gTrackCostFree == 0) {
        cost = LLCC_SystemGetKartTrackCost();
    }
    if (m == 0x10 && gTrackCostFree == 0) {
        cost = LLCC_SystemGetWaterTrackCost();
    }

    if (ValidTypeChange(mode) == 0) {
        return;
    }
    if (mType == mode) {
        ++mOverlapCounter;
        return;
    }

    int32_t canAfford = 1;
    if (gActionRecorder.mRecording == 0) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        const int32_t left = static_cast<int32_t>(static_cast<uint32_t>(balance) - cost);
        canAfford = (left >> 31) + 1;
    }
    if (m != 0x19 && (canAfford & 0xff) == 0 && m != 0 && m != 7) {
        return;
    }

    mType = mode;
    const uint32_t stamp = gGameWorld->GetMap()->MakeNewTimeStamp();
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id = {mCellId};
    gGameWorld->GetMap()->SetTimeStamp(stamp, id);

    if (IsJustTrack() != 0 && gLLCC4858 != 0) {
        CMapCell* const cell0 = GetMapCellEquiv();
        if (cell0 != nullptr) {
            MapDelta d1 = gMapDeltaPlusX;
            CMapCell* const cell1 = cell0->GetNeighbouringCell(&d1, 1);
            if (cell1 != nullptr) {
                MapDelta d2 = gMapDeltaPlusXPlusY;
                CMapCell* const cell2 = cell0->GetNeighbouringCell(&d2, 1);
                if (cell2 != nullptr) {
                    MapDelta d3 = gMapDeltaPlusY;
                    CMapCell* const cell3 = cell0->GetNeighbouringCell(&d3, 1);
                    if (cell3 != nullptr) {
                        const int32_t t0 = static_cast<int32_t>(cell0->mType);
                        const int32_t t1 = static_cast<int32_t>(cell1->mType);
                        const int32_t t2 = static_cast<int32_t>(cell2->mType);
                        const int32_t t3 = static_cast<int32_t>(cell3->mType);
                        if ((t0 == 0 || t0 == 1) && (t1 == 0 || t1 == 1) && (t2 == 0 || t2 == 1) && (t3 == 0 || t3 == 1)) {
                            cell0->Delete(0, 0);
                            cell1->Delete(0, 0);
                            cell2->Delete(0, 0);
                            cell3->Delete(0, 0);
                        }
                    }
                }
            }
        }
    }

    if (m != 0 && m != 7 && gTrackCostFree == 0) {
        gGameWorld->GetBankAccount()->MakeWithdrawal(cost);
    }
}

// 0x100a0b68, 16 bytes.
void CTrackCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) {
    mParentID = id.mIndex;
}

// CTrackCell::CalculateNeighbours (0x100a0bd4, 1,732 bytes): after a cell's mode changed, joins it to the neighbours that are track
// (modes 0xb ... ) or hoarding cells (type 0x19) in the four compass directions, when the argument offsets say 2 or -2.

int32_t gTrackNbrOn485C;
uint16_t gTrackNbr290;
uint16_t gTrackNbr292;
uint16_t gTrackNbr294;
uint16_t gTrackNbr296;
uint16_t gTrackNbr298;
uint16_t gTrackNbr29A;
uint16_t gTrackNbr29C;
uint16_t gTrackNbr29E;

namespace {

// One direction. `hoarding` selects the hoarding test (type 0x19) instead of IsTrack.
void Join(CTrackCell* self, const MapDelta& d, bool hoarding, const uint16_t& alt, uint8_t mine, uint8_t theirs) {
    MapDelta delta = d;
    CTrackCell* n = self->GetNeighbouringCell(delta, 2);
    if (n == nullptr) return;
    if (hoarding) {
        if (static_cast<int32_t>(n->mType) != 0x19) return;
    } else {
        if (n->IsTrack() == 0) return;
    }
    if (self->NumberOfNeighbours() >= 4) return;
    if (n->NumberOfNeighbours() >= 4) return;
    const uint16_t parent = n->mParentID;
    if (parent == gLLCCDB68 || parent == alt) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
    }
}

// The jump table at 0x1029e2a0, indexed by mode - 0xb: 1 track cells, 2 hoarding cells, 0 nothing.
const uint8_t kKind[15] = {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 2};

}  // namespace

void CTrackCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b) {
    int32_t x, y;
    if (gTrackNbrOn485C != 0) {
        if (gLLCCE288 == 0x80 || gLLCCE28C == 0x80) {
            x = 0;
            y = 0;
        } else {
            x = gLLCCE288;
            y = gLLCCE28C;
        }
    } else {
        x = a;
        y = b;
    }

    const uint32_t index = static_cast<uint32_t>(static_cast<int32_t>(mode) - 0xb);
    if (index <= 0xe && kKind[index] != 0) {
        const bool h = kKind[index] == 2;
        if (y == 2) Join(this, gMapDeltaMinusY, h, h ? gTrackNbr298 : gTrackNbr290, 1, 0x10);
        if (x == -2) Join(this, gMapDeltaPlusX, h, h ? gTrackNbr29A : gTrackNbr292, 4, 0x40);
        if (y == -2) Join(this, gMapDeltaPlusY, h, h ? gTrackNbr29C : gTrackNbr294, 0x10, 1);
        if (x == 2) Join(this, gMapDeltaMinusX, h, h ? gTrackNbr29E : gTrackNbr296, 0x40, 4);
    }

    gLLCCE288 = a;
    gLLCCE28C = b;
}

// Tile tables of the track cell modes (initial data of the Mac's data section).
#define TRACK_TILES(K) \
    {(K) + 2, 0, 0x11, {}}, {(K) + 2, 0x5a, 0x44, {}}, {(K) + 1, 0, 0x10, {}}, {(K) + 1, 0x5a, 0x40, {}}, \
    {(K) + 1, 0xb4, 0x01, {}}, {(K) + 1, 0x10e, 0x04, {}}, {(K) + 3, 0, 0x14, {}}, {(K) + 3, 0x5a, 0x50, {}}, \
    {(K) + 3, 0xb4, 0x41, {}}, {(K) + 3, 0x10e, 0x05, {}}, {(K) + 5, 0, 0x15, {}}, {(K) + 6, 0, 0x45, {}}, \
    {(K) + 7, 0, 0x51, {}}, {(K) + 8, 0, 0x54, {}}, {(K) + 4, 0, 0x55, {}}, {(K), 0, 0x00, {}}
CMapCellTileEntry gTrackTileTableA[16] = {TRACK_TILES(0x30000)};   // 0x1029d6e8, count gTrackTileCountA
int32_t gTrackTileCountA = 0x10;               // 0x1029d7a8
CMapCellTileEntry gTrackTileTableB[16] = {TRACK_TILES(0x40000)};   // 0x1029d7ac, count gTrackTileCountB
int32_t gTrackTileCountB = 0x10;               // 0x1029d86c
int32_t gTrackTileLast = 0;                    // 0x102e484c  the last random tile offset
int32_t gTrackTileRand = 0;                    // 0x102e4850  the next random value (0 to 0x1d)

// 0x100a12dc, 392 bytes.
void CTrackCell::CalculateDirections(int32_t a, int32_t b) {
    bool skip = false;
    if (gLLCC4818 != 0) {
        int32_t t = mType;
        if (t == 0xb || t == 0xc || t == 0x10 || t == 0xc || t == 0x11) skip = true;
        else if (GetMapCellEquiv()->mType == 0x15) skip = true;
    }
    if (skip) return;
    if (NumberOfHVNeighbours() > 2) {
        uint8_t d = GetDirection();
        if (a == -2) ModifyDirection(d | 0x40);
        if (a == 2) ModifyDirection(d | 4);
        if (b == -2) ModifyDirection(d | 1);
        if (b == 2) ModifyDirection(d | 0x10);
    } else {
        if (a == -2) ModifyDirection(0x40);
        if (a == 2) ModifyDirection(4);
        if (b == -2) ModifyDirection(1);
        if (b == 2) ModifyDirection(0x10);
    }
}

// CTrackCell::IsBridge 0x100a34c4, IsTrackStartBlock 0x100a36cc, GetSegmentNumber 0x100a399c, ValidTypeChange 0x100a149c (unit v5268).

namespace {

// The track cell table of the world: gGameWorld + 0x1102d8, 0x28 bytes each, indexed by parent id - 1 (the id is a halfword, 0 gives -1).
const CTrackCell* StartBlockCell(const CTrackCell* cell) {
    const uint16_t id = cell->mParentID;
    const uintptr_t base = reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8;
    const uintptr_t address = base + static_cast<uintptr_t>(static_cast<intptr_t>((static_cast<int32_t>(id) - 1) * 0x28 + 0x110000));
    return reinterpret_cast<const CTrackCell*>(address);
}

inline bool IsTrackXType(int32_t type) { return type == 0xc || type == 0x11; }

}  // namespace

// 0x100a149c: the map cell lookup's answer is unused.
int32_t CTrackCell::ValidTypeChange(CELL_MODES mode) {
    GetMapCellEquiv();
    const int32_t type = static_cast<int32_t>(mType);
    if (type == 0x19) return 1;
    if (type == 7) return 1;
    if (IsFlag(0x40) != 0) return 1;
    if (static_cast<int32_t>(mode) == 0) return 1;
    if (type == 0) return 1;
    if (static_cast<int32_t>(mode) == type) return 1;
    return 0;
}

// Unit v5629: CTrackCell::ConnectionType (0x100a1594, 4,932 bytes), how a cell of mode `mode` would join this track cell (the cell
// system's placement test). It also owns five cell system words no other file names (see TrackCell.h).

// gLLCCC20C, gLLCCDB60, gLLCCE0B4, gLLCCE0B8 are defined by other units now.
int32_t gLLCCDB40 = 0;   // 0x102ddb40

namespace {

// The ride rule of one map cell: a ride part (mode 4, 9 or 10) whose ride ranks below gLLCCE0B8 forbids the placement (answers false).
// The ride's rank (kRideSlot_OverwritePriority, the record's word 0x54) is left in gLLCCE0B4 when the cell's thing is an object.
bool RideAllows(const CMapCell* cell) {
    const int32_t type = static_cast<int32_t>(cell->mType);
    if (type != 4 && type != 9 && type != 10) return true;
    const CMapCell* parent = &gGameWorld->mMapCells[static_cast<int32_t>(cell->mParentID) - 1];
    const uint16_t id = parent->GetObjectThingId().mIndex;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    const CThing* thing = static_cast<const CThing*>(gThingArray[id].mThing);
    if (thing->mKind == 3) {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ride;
        ride.mIndex = static_cast<const CObject*>(gThingArray[id].mThing)->mObjectType;
        gLLCCE0B4 = gRideDatabase.GetRideInfo(ride)->Value(kRideSlot_OverwritePriority).mInteger;
    }
    return !(gLLCCE0B8 > gLLCCE0B4);
}

bool IsSpecial(CMapCell* cell) {
    return cell->IsRide() != 0 || cell->IsFreeform() != 0 || cell->IsExit() != 0 || cell->IsEntrance() != 0 ||
           static_cast<int32_t>(cell->mType) == 0x15;
}

// A track neighbour of another segment (a track-X cell stands for the cell of its parent id) blocks the cell.
bool OtherSegment(CTrackCell* cell) {
    if (static_cast<int32_t>(cell->mType) == 0) return false;
    if (cell->IsTrackX() != 0) cell = &gGameWorld->mTrackCells[static_cast<int32_t>(cell->mParentID) - 1];
    if (cell->IsTrack() != 0 && cell->mParentID != gLLCCDB68) {
        gLLCCC214 = 1;
        return true;
    }
    return false;
}

}  // namespace

int32_t CTrackCell::ConnectionType(CELL_MODES modeArg, int32_t a, uint8_t b) {
    const int32_t mode = static_cast<int32_t>(modeArg);
    bool blocked = false;
    int32_t cost = 0;

    if (mode == 0x39) return gLLCCC214 != 0 ? 1 : 0;

    CMapCell* m0 = GetMapCellEquiv();
    MapDelta d1 = gMapDeltaPlusX;
    CMapCell* m1 = m0->GetNeighbouringCell(&d1, 1);
    MapDelta d2 = gMapDeltaPlusXPlusY;
    CMapCell* m2 = m0->GetNeighbouringCell(&d2, 1);
    MapDelta d3 = gMapDeltaPlusY;
    CMapCell* m3 = m0->GetNeighbouringCell(&d3, 1);

    if (IsSpecial(m0) || IsSpecial(m1) || IsSpecial(m2) || IsSpecial(m3)) blocked = true;

    if (m0->IsFlag(0x40) != 0 || m1->IsFlag(0x40) != 0 || m2->IsFlag(0x40) != 0 || m3->IsFlag(0x40) != 0) return 1;

    // The original tests m2 twice and never m3.
    if (!RideAllows(m0)) return 0;
    if (!RideAllows(m1)) return 0;
    if (!RideAllows(m2)) return 0;
    if (!RideAllows(m2)) return 0;

    if (mode == 0xb) cost = static_cast<int32_t>(LLCC_SystemGetKartTrackCost());
    if (mode == 0x10) cost = static_cast<int32_t>(LLCC_SystemGetWaterTrackCost());
    if (mode == 0xb || mode == 0x10) {
        CTrackCell* t1 = GetNeighbouringCell(gMapDeltaPlusX, 1);
        CTrackCell* t2 = GetNeighbouringCell(gMapDeltaPlusXPlusY, 1);
        CTrackCell* t3 = GetNeighbouringCell(gMapDeltaPlusY, 1);
        if (OtherSegment(t1)) blocked = true;
        if (OtherSegment(t2)) blocked = true;
        if (OtherSegment(t3)) blocked = true;
    }
    if (gLLCCC210 != 0) gLLCCC214 = 1;
    if (a == 0 && gLLCCC214 != 0) return 1;

    int32_t type = static_cast<int32_t>(mType);
    if (type == 0x19 && mode == 0x19) {
        const uint32_t index = static_cast<uint32_t>(mCellId) - 1;
        const int32_t x = static_cast<int32_t>(index & 0x7f);
        const int32_t y = static_cast<int32_t>(index >> 7);
        if (x == gHoardingRight && y == gHoardingBottom) return 1;
        if (x == gHoardingLeft && y == gHoardingTop) return 1;
        return 0;
    }
    if (mode == 0x19) {
        if (type == 0x19) {   // unreachable: the test above took mode and type 0x19
            if (b != OPPOSITE(mDirection)) return 0;
            gLLCCC214 = 1;
            return 1;
        }
        if (type == 0 || type == 7) return 0;
        gLLCCC214 = 1;
        return 1;
    }
    if (mode == type && mParentID == gLLCCDB68) {
        if (gLLCCC20C == 0 || NumberOfNeighbours() == 1 || NumberOfNeighbours() == 3) gLLCCC20C = 1;
    }
    if (mode == static_cast<int32_t>(mType) && gTrackNbrOn485C == 0 && gLLCC4858 == 0 && IsStraight() == 0 &&
        NumberOfNeighbours() != 1) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }

    GetMapCellEquiv();   // the answer is unused
    bool costs;
    type = static_cast<int32_t>(mType);
    if (type == 0x19 || type == 7 || IsFlag(0x40) != 0 || mode == 0) {
        costs = true;
    } else {
        type = static_cast<int32_t>(mType);
        costs = type == 0 || mode == type;
    }
    if (costs && static_cast<int32_t>(mType) != mode && gTrackCostFree == 0) LLCC_SystemAddToSessionCost(cost);

    if (gActionRecorder.mRecording == 0 && mode != 0xe && mode != 0x13) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        if (LLCC_SystemGetSessionCost() > balance) {
            if (a == 0) {
                gLLCCC214 = 1;
                LLCM_SetIllegalPlacement(1);
            }
            gLLCCC204 = 1;
            return 1;
        }
    }

    type = static_cast<int32_t>(mType);
    if (gLLCC4858 != 0 && type != 0xb && type != 0x10 && (mode == 0xb || mode == 0x10) &&
        (m0->IsQueue() != 0 || m1->IsQueue() != 0 || m2->IsQueue() != 0 || m3->IsQueue() != 0 || m0->IsFlag(0x20) != 0 ||
         m1->IsFlag(0x20) != 0 || m2->IsFlag(0x20) != 0 || m3->IsFlag(0x20) != 0)) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }

    type = static_cast<int32_t>(mType);
    if (type == 0 && mode != 0xe && mode != 0x13 && !blocked) {
        if (a == 0) gLLCCC20C = 0;
        return 0;
    }

    if (mode == type || (mode == 0xb && type == 0xd) || (mode == 0x10 && type == 0x12)) {
        if ((gTrackNbrOn485C != 0 && gLLCCDB60 != 0) || gLLCC4858 != 0) {
            if (type == 0xb || type == 0x10) {
                if (b != mDirection && NumberOfNeighbours() == 2) {
                    if (a == 0) gLLCCC210 = 1;
                    return 1;
                }
                if (IsCorner() != 0) {
                    if (a == 0) gLLCCC210 = 1;
                    return 1;
                }
            }
            type = static_cast<int32_t>(mType);
            if ((type == 0xd || type == 0x12) && b != mDirection) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            if (IsBridge() != 0 && b != 0 && b != mDirection && (gLLCC4858 != 0 || gTrackNbrOn485C != 0)) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            if (b != OPPOSITE(mDirection) && mParentID == gLLCCDB68 && IsFlag(0x10) != 0 && b != mDirection) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            gLLCCC200 = 1;
            return 0xb;
        }
        if (IsBridge() != 0 && b != 0 && b != mDirection && (gLLCC4858 != 0 || gTrackNbrOn485C != 0)) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
    }

    if (static_cast<int32_t>(mType) == 0xb && mode == 0xb && b != OPPOSITE(mDirection) && mParentID == gLLCCDB68) {
        if (IsFlag(0x10) != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        if (IsBridge() != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        return 0;
    }
    type = static_cast<int32_t>(mType);
    if (type == 0x12 && mode == 0x10 && b == mDirection) {
        if (a == 0) gLLCCC210 = 1;
        return 0;
    }
    if (type == 0x10 && mode == 0x10 && b != OPPOSITE(mDirection) && mParentID == gLLCCDB68) {
        if (IsFlag(0x10) != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        if (IsBridge() != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        return 0;
    }
    if (static_cast<int32_t>(mType) == 0xd && mode == 0xb && b == mDirection) {
        if (a == 0) gLLCCC210 = 1;
        return 0;
    }
    if (blocked && IsFlag(0x10) == 0) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }
    if (mode == 0xf) {
        type = static_cast<int32_t>(mType);
        if ((type == 0xb || type == 0x10) && mParentID == gLLCCDB68) return 0xd;
        if (type == 0x19) return 0xd;
        return 1;
    }
    if (static_cast<int32_t>(mType) == 0xb && (mode == 0xe || mode == 0x13) && mParentID == gLLCCDB68 && IsCrossRoad() == 0) return 9;
    if (static_cast<int32_t>(mType) == 0x10 && (mode == 0x13 || mode == 0xe) && mParentID == gLLCCDB68 && IsCrossRoad() == 0) return 9;
    if (IsCorner() != 0 && gLLCCDB40 == 0) {
        if (a == 0) gLLCCC214 = 1;
        return 1;
    }
    if (IsBridge() != 0) {
        bool fire;
        if (gTrackNbrOn485C != 0 && b != mDirection && b != OPPOSITE(mDirection)) {
            fire = true;
        } else {
            fire = gTrackNbrOn485C == 0;
        }
        if (fire) {
            if (a == 0) gLLCCC214 = 1;
            return 1;
        }
    }
    if (NumberOfNeighbours() == 2 && b != mDirection && b != OPPOSITE(mDirection)) gLLCCC20C = 0;
    if (mode == static_cast<int32_t>(mType) && mParentID == gLLCCDB68 && NumberOfNeighbours() == 3) return 0;
    return 1;
}

// CTrackCell::IsFlag (main app, SimTheme Park 2.04b3, 0x100a2918, 32 bytes; unit w206): whether any bit of `flag` (its low halfword) is set in the
// cell's flag word. The twin of CMapCell::IsFlag (src/MapCell_flags.cpp, unit w201).

int32_t CTrackCell::IsFlag(uint16_t flag) const { return (mFlags & flag) != 0 ? 1 : 0; }

// CTrackCell's flag word and kind tests (main app, SimTheme Park 2.04b3; the inventory files this code under lobby.cpp / TrainingCentre.cpp: unreliable;
// unit w201): GetFlag 0x100a2998, SetFlag 0x100a2964, UnSetFlag 0x100a29cc, IsHoarding 0x100a35dc, IsTrackX 0x100a367c and IsCorner 0x100a37a0.
// SetFlag REPLACES the flag word (it does not OR the bits in): LLCC_LockHoarding reads the word, ORs 0x20 and stores it back.

// 0x100a2964, 8 bytes.
void CTrackCell::SetFlag(uint16_t flags) { mFlags = flags; }

// 0x100a2998, 8 bytes.
uint16_t CTrackCell::GetFlag() { return mFlags; }

// 0x100a29cc, 16 bytes.
void CTrackCell::UnSetFlag(uint16_t flags) { mFlags = static_cast<uint16_t>(mFlags & ~flags); }

namespace {
// Jump table 0x1029e2dc: 0 default (table B), 1 plain track table, 2 straight-track table (0x1029d6e8), 3 the other table (0x1029d7ac), 4 hoarding.
const uint8_t kKind_2[26] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 4};
}

// 0x100a2a0c, 1728 bytes.
uint32_t CTrackCell::GetTile(int32_t* rotation) const {
    uint8_t nb = GetNeighbours();
    int32_t mode = mType;
    uint8_t dir = GetDirection();
    GetMapCellEquiv();
    int32_t rot = 0;
    *rotation = 0;
    int32_t m = mode < 0 ? -mode : mode;
    if (m == 5 || m == 7 || m == 4 || m == 0xc || m == 0x11 || m == 9 || m == 10) return 8;

    const CMapCellTileEntry* table;
    int32_t count;
    int32_t tile = -1;
    int32_t x = 0, y = 0;
    int kind = (static_cast<uint32_t>(m) <= 0x19) ? kKind_2[m] : 0;
    switch (kind) {
        default:
            table = gTileTableB; count = gTileCountB;
            break;
        case 1:
            table = gTrackTileTableA; count = gTrackTileCountA;
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            break;
        case 3:
            table = gTrackTileTableB; count = gTrackTileCountB;
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            break;
        case 4: {
            table = gTrackTileTableB; count = gTrackTileCountB;
            uint32_t id = mCellId;
            x = static_cast<int32_t>((id - 1) & 0x7f);
            y = static_cast<int32_t>((id - 1) >> 7);
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            if ((x == gHoardingRight && y == gHoardingBottom) || (x == gHoardingLeft && y == gHoardingTop))
                nb = nb | OPPOSITE(nb);
            break;
        }
    }
    bool found = false;
    for (int32_t i = 0; i < count && !found; i++) {
        if (nb == table[i].mMask) {
            found = true;
            tile = static_cast<int32_t>(table[i].mTile);
            *rotation = rot + table[i].mRotation;
        }
    }
    if (tile == -1) {
        found = false;
        for (int32_t i = 0; i < count && !found; i++) {
            uint8_t mk = table[i].mMask;
            if (mk == (nb & mk)) {
                found = true;
                tile = static_cast<int32_t>(table[i].mTile);
                *rotation = rot + table[i].mRotation;
            }
        }
    }
    if (!IsCorner() && !IsCrossRoad()) {
        uint16_t f = mFlags;
        if (f & 1) tile += 9;
        if (f & 2) tile += 0xc;
        if (f & 8) tile += 0xf;
        if (f & 4) tile = static_cast<int32_t>((static_cast<uint32_t>(tile) & 0xffff0000u) + 0x12);
    }
    if ((mode == 0x10 || mode == 0xb) && IsCorner()) {
        uint8_t d = GetDirection();
        uint8_t n = GetNeighbours();
        if ((d == 0x40 && n == 0x50) || (d == 0x10 && n == 0x14) || (d == 1 && n == 0x41) || (d == 4 && n == 5))
            tile += 0x10;
    }
    if (mode == 0x10 && IsCrossRoad()) {
        uint8_t d = GetDirection();
        if (d == 0x14) *rotation = 0x5a;
        if (d == 0x41) tile += 0x10;
        if (d == 0x50) {
            tile += 0x10;
            *rotation = 0x10e;
        }
    }
    if (mode == 0x19) {
        if (IsCorner()) {
            uint8_t d = GetDirection();
            uint8_t n = GetNeighbours();
            if ((d == 1 && n == 0x41) || (d == 0x10 && n == 0x14) || (d == 4 && n == 5) || (d == 0x40 && n == 0x50) ||
                (d == 0x40 && n == 0x14) || (d == 1 && n == 0x50) || (d == 4 && n == 0x41) || (d == 0x10 && n == 5)) {
                tile += 0x10;
                if (IsBridge() != 0) tile = 0x1b;
            } else if (IsBridge() != 0) {
                tile = 0x1a;
            }
        }
        if (IsStraight() && IsBridge() == 0) {
            // r20 is the caller's register when the counter is 0x1e or more (never: it is a remainder of 0x1e).
            int32_t r20 = 0;
            int32_t c = gTrackTileRand;
            if (c < 0x12) r20 = 0;
            else if (c < 0x18) r20 = 0x14;
            else if (c < 0x1e) r20 = 0x17;
            if (gTrackTileLast != r20) tile += r20;
            gTrackTileLast = r20;
            gTrackTileRand = static_cast<int32_t>(gGameWorld->GetRand() % 0x1e);
        }
        tile = static_cast<int32_t>((static_cast<uint32_t>(tile) & 0xffff) | 0x50000);
    }
    return static_cast<uint32_t>(tile);
}

// 0x100a30f8 (912 bytes): draws a track line of mode `mode` from (x, y) along the longer axis, two cells per step; the end is
// moved to an even distance. gTrackNbrOn485C is 1 during the call; gLLCC4858 is 1 for the last cell.
int32_t LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    gTrackNbrOn485C = 1;
    gLLCC4858 = 0;
    const int32_t dy = std::abs(*endY - y);
    const int32_t dx = std::abs(*endX - x);
    if (dx > dy) {
        const int32_t step = Snap(x, endX);
        while (x != *endX) {
            if (CMap::IsXYValid(x, y) != 0) {
                if (CellAt(x, y)->ModifyBody(mode, step, 0) == 0) {
                    gTrackNbrOn485C = 0;
                    return 0;
                }
                gTrackNbrOn485C = 0;
            }
            x += step;
        }
        gLLCC4858 = 1;
        if (CMap::IsXYValid(x, y) != 0) {
            if (CellAt(x, y)->ModifyBody(mode, step, 0) == 0) {
                gTrackNbrOn485C = 0;
                gLLCC4858 = 0;
                return 0;
            }
            gTrackNbrOn485C = 0;
            gLLCC4858 = 0;
        }
    } else {
        const int32_t step = Snap(y, endY);
        while (y != *endY) {
            if (CMap::IsXYValid(x, y) != 0) {
                if (CellAt(x, y)->ModifyBody(mode, 0, step) == 0) {
                    gTrackNbrOn485C = 0;
                    return 0;
                }
                gTrackNbrOn485C = 0;
            }
            y += step;
        }
        gLLCC4858 = 1;
        if (CMap::IsXYValid(x, y) != 0) {
            if (CellAt(x, y)->ModifyBody(mode, 0, step) == 0) {
                gTrackNbrOn485C = 0;
                gLLCC4858 = 0;
                return 0;
            }
            gTrackNbrOn485C = 0;
            gLLCC4858 = 0;
        }
    }
    return 1;
}

// 0x100a34c4: any of the four low flag bits.
int32_t CTrackCell::IsBridge() const {
    const uint16_t flags = mFlags;
    if ((flags & 1) != 0 || (flags & 2) != 0 || (flags & 4) != 0 || (flags & 8) != 0) return 1;
    return 0;
}

// 0x100a3574, 60 bytes: the mode is 0xb, 0xd, 0x10, 0x12 or 0x19.
int32_t CTrackCell::IsTrack() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 0xb || t == 0xd || t == 0x10 || t == 0x12 || t == 0x19) return 1;
    return 0;
}

// 0x100a35dc, 28 bytes: the cell's mode is 0x19.
int32_t CTrackCell::IsHoarding() const { return mType == 0x19 ? 1 : 0; }

// 0x100a3628, 36 bytes: the mode is 0xb or 0x10.
int32_t CTrackCell::IsJustTrack() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 0xb || t == 0x10) return 1;
    return 0;
}

// 0x100a367c, 36 bytes: the cell's mode is 0xc, else 0x11.
int32_t CTrackCell::IsTrackX() const {
    if (mType == 0xc) return 1;
    return mType == 0x11 ? 1 : 0;
}

// 0x100a36cc: for a track X cell, the cell its parent id names counts when it is type 0xd or 0x12; then the cell itself.
int32_t CTrackCell::IsTrackStartBlock() const {
    if (IsTrackXType(static_cast<int32_t>(mType))) {
        const CTrackCell* parent = StartBlockCell(this);
        if (parent != nullptr) {
            const int32_t type = static_cast<int32_t>(parent->mType);
            if (type == 0xd || type == 0x12) return 1;
        }
    }
    const int32_t type = static_cast<int32_t>(mType);
    if (type == 0xd || type == 0x12) return 1;
    return 0;
}

// 0x100a37a0, 156 bytes: the neighbour byte (GetNeighbours is read first) when exactly two of the four horizontal and vertical neighbours are set and
// they are at a right angle; the tests run in the listing's order: 1 and 4, 1 and 0x40, 0x10 and 4, 0x10 and 0x40. Else 0.
uint8_t CTrackCell::IsCorner() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 1) != 0 && (neighbours & 0x40) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// CTrackCell's shape tests the track walks ask (main app, SimTheme Park 2.04b3; lobby.cpp on the Mac, the inventory's file for this range;
// unit w204): IsStraight 0x100a3868 and IsCrossRoad 0x100a3914. Both read the neighbour byte first (GetNeighbours), then count the horizontal and
// vertical neighbours (NumberOfHVNeighbours); the neighbour byte is the answer when the shape fits, else 0. The compass bits: 1 north (y - 1),
// 4 east (x + 1), 0x10 south (y + 1), 0x40 west (x - 1).

// 0x100a3868, 124 bytes: exactly two horizontal / vertical neighbours that lie in one line (1 with 0x10, or 4 with 0x40).
uint8_t CTrackCell::IsStraight() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 0x10) != 0) return neighbours;
    if ((neighbours & 4) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// 0x100a3914, 88 bytes: more than two horizontal / vertical neighbours (a signed compare).
uint8_t CTrackCell::IsCrossRoad() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() > 2) return neighbours;
    return 0;
}

// 0x100a399c: the segment number of the parent cell for a track X cell, else the cell's own.
int16_t CTrackCell::GetSegmentNumber() const {
    if (IsTrackXType(static_cast<int32_t>(mType))) return StartBlockCell(this)->mSegmentNumber;
    return mSegmentNumber;
}

// 0x100a3a38, 8 bytes.
void CTrackCell::SetSegmentNumber(int16_t number) {
    mSegmentNumber = number;
}

// The map reset (main app of SimTheme Park 2.04b3; the inventory files LLCC_ResetMap under LLCellSystem.cpp, HFieldCellStatus under
// TPRide.cpp, RemoveNeighbour and ModifyDirection under LLCellMouse.cpp and the cell resets under unrelated names; unit w112c):
// LLCC_ResetMap puts every cell of the map and of the track layer back to its start state (the cell's place comes from its id, the
// start state from the height field and, for the map cells, the attribute map).

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

int32_t HFieldCellStatus(int32_t x, int32_t y);

// CTrackCell::SetHFieldTile (0x100a3bac), RefreshHFieldTile (0x100a3ec0) and QuickDelete (0x100a4e88), main app.

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

// 0x100a4048, 68 bytes.
void CTrackCell::RemoveInstanceMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyKTrackMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

// CTrackCell::Delete 0x100a40c4 (3,480 bytes): takes a track, kart, water or hoarding cell out of the track system.

namespace {

// The cell of a parent id: id - 1 cells into the world's track cells (0x1102d8), computed in 32 bits (id 0 lands one cell before).
inline CTrackCell* TrackCellOf(uint16_t id) {
    return gGameWorld->mTrackCells + (static_cast<int32_t>(id) - 1);
}

inline bool IsTrackXType_2(int32_t t) { return t == 0xc || t == 0x11; }
inline bool IsRideType(int32_t t) { return t == 0xb || t == 0xd || t == 0x10 || t == 0x12 || t == 0x19; }

// Whether the cell, or the cell its track part belongs to, is a ride cell of kind 0xd or 0x12 (inlined in the original).
inline bool IsRideEnd(CTrackCell* n) {
    if (IsTrackXType_2(n->mType)) {
        CTrackCell* p = TrackCellOf(n->mParentID);
        if (p != nullptr && (p->mType == 0xd || p->mType == 0x12)) return true;
    }
    return n->mType == 0xd || n->mType == 0x12;
}

}  // namespace

// The jump table at 0x1029e344 (15 words, mode - 0xb): 1 where the case body at 0x100a4240 runs, 0 where it goes to 0x100a4ad8.
static const uint8_t kCaseBody[15] = {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1};

void CTrackCell::Delete(int32_t a, int32_t b, bool c) {
    uint16_t first = 0;   // r29
    uint16_t last = 0;    // r23
    uint32_t cost = 0;    // r25
    uint8_t dir;
    uint8_t nb;
    bool corner;
    CTrackCell* n;

    if (mType == 0x10) {
        cost = LLCC_SystemGetWaterTrackCost();
    } else if (mType == 0xb) {
        cost = LLCC_SystemGetKartTrackCost();
    }

    if (IsTrackXType_2(mType)) {
        TrackCellOf(mParentID)->Delete(a, b, true);
        goto L4ad8;
    }

    if (CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY)) {
        first = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCFirstBlockX) +
                                      (static_cast<uint32_t>(gLLCCFirstBlockY) << 7) + 1);
    }
    if (CMap::IsXYValid(gLLCCE034, gLLCCE038)) {
        last = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCE034) +
                                     (static_cast<uint32_t>(gLLCCE038) << 7) + 1);
    }

    {
        const uint32_t index = static_cast<uint32_t>(mType) - 0xb;
        if (index > 0xe || kCaseBody[index] == 0) goto L4ad8;
    }

    if (a == 0 && b == 0 && mCellId != first) {
        mOverlapCounter = -1;
        goto L4560;
    }
    mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
    if (a == -1 && b == -1) {
        switch (GetDirection()) {
            case 1:
            case 0x10:
                RemoveNeighbour(4);
                RemoveNeighbour(0x40);
                break;
            case 4:
            case 0x40:
                RemoveNeighbour(1);
                RemoveNeighbour(0x10);
                break;
            default:
                break;
        }
        SetHFieldTile();
        goto L4560;
    }
    if (NumberOfDirections() >= 2) {
        if (a == -2) RemoveDirection(4);
        if (a == 2) RemoveDirection(0x40);
        if (b == -2) RemoveDirection(0x10);
        if (b == 2) RemoveDirection(1);
        if (std::abs(a) == 2) {
            RemoveNeighbour(4);
            RemoveNeighbour(0x40);
        }
        if (std::abs(b) == 2) {
            RemoveNeighbour(0x10);
            RemoveNeighbour(1);
        }
        SetHFieldTile();
        goto L4560;
    }
    if (mCellId == first || mCellId == last) goto L4560;

    corner = false;
    nb = GetNeighbours();
    if (NumberOfHVNeighbours() == 2) {
        const bool side = (nb & 4) != 0 || (nb & 0x40) != 0;
        corner = ((nb & 1) != 0 && side) || ((nb & 0x10) != 0 && side);
    }
    if (a == -2) RemoveNeighbour(4);
    if (a == 2) RemoveNeighbour(0x40);
    if (b == -2) RemoveNeighbour(0x10);
    if (b == 2) RemoveNeighbour(1);
    if (corner && NumberOfNeighbours() == 1) {
        ModifyDirection(OPPOSITE(GetNeighbours()));
    }
    SetHFieldTile();

L4560:
    if (mOverlapCounter < 0 && mCellId != first && mCellId != last) goto L45c4;
    if (mOverlapCounter < 0 && gLLCC47E0 != 0) goto L45c4;
    if (gLLCC47E4 == 0) goto L4ad8;

L45c4:
    mOverlapCounter = 0;
    if (mType != 0x19 && gActionRecorder.mRecording == 0) {
        CMapCell* mapCell = TrackCellOf(mParentID)->GetMapCellEquiv();
        auto thingId = mapCell->GetObjectThingId();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr,
                       "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        const int32_t percentage = static_cast<CObject*>(gThingArray[thingId.mIndex].mThing)->GetResidualValuePercentage();
        CBankAccount* bank = gGameWorld->GetBankAccount();
        const uint32_t product = cost * static_cast<uint32_t>(percentage);
        // x / 100 as the compiler writes it: mulhwu by 0x51eb851f, then a shift of 5.
        bank->MakeDeposit(static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * product) >> 32) >> 5);
    }

    dir = 1;
    do {
        if ((GetNeighbours() & dir) != 0) {
            RemoveNeighbour(dir);
            n = GetNeighbouringCell(dir, 2);
            if (n != nullptr) {
                n->RemoveNeighbour(OPPOSITE(dir));
                n->SetHFieldTile();
                if (n->NumberOfNeighbours() == 1) {
                    uint8_t neighbours = n->GetNeighbours();
                    if (n->GetDirection() != neighbours) {
                        neighbours = OPPOSITE(n->GetNeighbours());
                        if (n->GetDirection() != neighbours) {
                            n->ModifyDirection(OPPOSITE(n->GetNeighbours()));
                        }
                    }
                }
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);

    if ((mFlags & 0x10) != 0) {
        const int32_t id = static_cast<int32_t>(mCellId) - 1;
        LLCR_DeleteRide(id & 0x7f, static_cast<int32_t>(static_cast<uint32_t>(id) >> 7), 1);
    }
    ModifyDirection(0);
    mFlags = 0;
    ModifyType(static_cast<CELL_MODES>(0));
    if (mType != 0x19) {
        mParentID = 0;
        mSegmentNumber = -1;
    }
    SetHFieldTile();

    n = GetNeighbouringCell(gMapDeltaPlusY, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }
    n = GetNeighbouringCell(gMapDeltaPlusX, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }
    n = GetNeighbouringCell(gMapDeltaPlusXPlusY, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }

L4ad8:
    if (mCellId == first && IsRideType(mType) && gLLCC47E0 == 0) {
        dir = 1;
        do {
            if ((GetNeighbours() & dir) == 0) {
                n = GetNeighbouringCell(dir, 2);
                if (n != nullptr) {
                    if (IsRideEnd(n) && n->mParentID == mParentID) {
                        ModifyDirection(OPPOSITE(dir));
                        ModifyNeighbours(dir);
                        SetHFieldTile();
                    }
                    if (!IsRideType(n->mType)) {
                        RemoveNeighbour(dir);
                        SetHFieldTile();
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
        dir = 1;
        if (NumberOfNeighbours() == 1) {
            RemoveDirection(0xff);
            do {
                if ((GetNeighbours() & dir) != 0) {
                    n = GetNeighbouringCell(dir, 2);
                    if (n != nullptr && IsRideEnd(n) && n->mParentID == mParentID) {
                        ModifyDirection(OPPOSITE(dir));
                    }
                }
                dir = ADD_DIRECTION(dir, 4);
            } while (dir != 1);
        }
    }
    if (mType != 0 && mMeshInstance == 0) SetHFieldTile();
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

// 0x100a5098, 68 bytes.
void CTrackCell::DeleteMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyKTrackMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

namespace {

CTrackCell* TrackCellAt(uint16_t id) {
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];   // (an id of 0 reads the cell before the grid)
}

// The tail the two removal blocks share: take the bits off the cell, then off the cell beyond.
void BreakBits(CTrackCell* cell, uint8_t bits) {
    cell->RemoveNeighbour(bits);
    cell->SetHFieldTile();
    if (cell->GetOverlapCounter() != 0) cell->DecOverlapCounter();
    CTrackCell* beyond = cell->GetNeighbouringCell(bits, 2);
    if (beyond != nullptr) {
        beyond->RemoveNeighbour(OPPOSITE(bits));
        beyond->SetHFieldTile();
    }
}

}  // namespace

// 0x100a510c (1156 bytes)
CTrackCell* LLCC_BreakTrack(int32_t x, int32_t y, uint8_t dir) {
    uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1);
    CTrackCell* cell = TrackCellAt(id);
    uint8_t bits = cell->GetDirection();
    if (x == gLLCCE034 && y == gLLCCE038) {
        bits = static_cast<uint8_t>(cell->GetNeighbours() & ~bits);
        if (bits != 0) {
            cell->RemoveNeighbour(bits);
            cell->SetHFieldTile();
            if (cell->GetOverlapCounter() != 0) cell->DecOverlapCounter();
            CTrackCell* beyond = cell->GetNeighbouringCell(bits, 2);
            if (beyond != nullptr) {
                beyond->RemoveNeighbour(OPPOSITE(bits));
                beyond->SetHFieldTile();
            }
        }
        return TrackCellAt(id);
    }
    if (dir != 0) bits = dir;
    if (bits != 0) BreakBits(cell, bits);

    const int32_t fx = gLLCCFirstBlockX;
    if (((x + 2 == fx && y == gLLCCFirstBlockY) || (x == fx && y + 2 == gLLCCFirstBlockY) ||
         (x - 2 == fx && y == gLLCCFirstBlockY) || (x == fx && y - 2 == gLLCCFirstBlockY)) &&
        CMap::IsXYValid(fx, gLLCCFirstBlockY) != 0) {
        id = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCFirstBlockX) + (static_cast<uint32_t>(gLLCCFirstBlockY) << 7) + 1);
        CTrackCell* c = TrackCellAt(id);
        static const uint8_t kDirs[4] = {1, 4, 0x10, 0x40};
        static const int32_t kTypes[2] = {0xd, 0x12};
        for (int t = 0; t < 2; ++t) {
            for (int i = 0; i < 4; ++i) {
                if (static_cast<int32_t>(c->GetNeighbouringCell(kDirs[i], 2)->mType) == kTypes[t]) {
                    c->ModifyNeighbours(kDirs[i]);
                    c->SetHFieldTile();
                }
            }
        }
    }
    return TrackCellAt(id);
}

// 0x100a55bc (224 bytes)
uint32_t LLCC_GetTrackMeshAt(int32_t x, int32_t y) {
    const uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1);
    CTrackCell* cell = TrackCellAt(id);
    const int32_t type = static_cast<int32_t>(cell->mType);
    if (type == 0xc || type == 0x11) {
        const uint16_t parent = cell->mParentID;
        if (parent == gLLCCE380) {
            CTRACE(6, "NULL mesh from TrackX... shouldnt happen!!\n");
            return 0;
        }
        cell = TrackCellAt(parent);
    }
    return cell->mMeshInstance;
}
