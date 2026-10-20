// CTrackCell::Modify (0x1009dfe4, 10,252 bytes, unit v5881): the track cell system's one entry for a change to a track or hoarding cell.
// Bit 0x100 of the mode draws the cell's blueprint (a square face at the cell's height plus wall faces down to every lower neighbour);
// otherwise the low byte picks the action: 0x32 deletes, 0x80 joins the neighbours, 0x81 fixes the flags and tiles of the hoarding,
// 0x82 the directions, 0x83 hands the cell's parent id to three neighbours, 0x85 fixes the flags against the water around, 0x86 fixes
// the bridges of the ride on the cell, 0x87 deletes the rides under the cell, and any other mode changes the cell's type.
// The Mac answers r3 (ModifyBody below); the declared Modify is void and passes the call on.
#include <cstdint>
#include "TrackCell.h"

#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

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
