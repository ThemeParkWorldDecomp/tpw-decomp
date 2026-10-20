// CMapCell::Modify (0x10097538, 8360 bytes; unit v5637). The cell system's one entry for changing a map cell: bit 0x200 of the mode only
// asks whether the placement would connect, bit 0x100 draws the placement preview (the square and its walls down to the four neighbours),
// and the low byte otherwise picks a command (0x80 to 0x87, 0x32) or a mode to set through ModifyType.
#include <cstddef>
#include <cstdint>
#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "TPCamera.h"
#include "World.h"
#include "MainBalance.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Debug.h"
#include "rsse.h"
#include "RegionEffectCell.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "ThingReceivers.h"
#include "ppc_math.h"

// The globals of this function (names invented; all start at 0).
int32_t gModifyLastMode;      // 0x102dc21c
int32_t gModifyTypeRefused;   // 0x102e483c
int32_t gModifyPylonIndex;    // 0x102ddb70
uint16_t gModifyNoParent;     // 0x1029e20e
uint16_t gModifyNoThing;      // 0x1029e210

namespace {

constexpr const char* kModifyThingZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023e42d

// The ground height of the square rounded up to a multiple of 10 (fctiwz, then (i + 9) / 10 truncating, times 10.0f).
float SquareHeight(int32_t x, int32_t y) {
    const float height = GetWorldGSquareHeight(x, y, nullptr);
    const int32_t rounded = ppc_fctiwz(static_cast<double>(height)) + 9;
    return 10.0f * static_cast<float>(rounded / 10);
}

// The height a cell's preview starts from: 0, or the square's height when its track cell is a raised kind (0xb, 0xc, 0x10, 0x11, 0x19) and
// again when the cell's own mode is 4, 7, 9, 10 or 0x1e (the height is asked twice then, as the original does).
float CellHeight(CMapCell* cell, int32_t x, int32_t y) {
    float height = 0.0f;
    const int32_t track = static_cast<int32_t>(cell->GetTrackCellEquiv()->mType);
    if (static_cast<uint32_t>(track - 0xb) <= 1 || static_cast<uint32_t>(track - 0x10) <= 1 || track == 0x19) height = SquareHeight(x, y);
    const int32_t type = static_cast<int32_t>(cell->mType);
    if (type == 7 || type == 0x1e) {
        height = SquareHeight(x, y);
    } else if (type == 4 || static_cast<uint32_t>(type - 9) <= 1) {
        height = SquareHeight(x, y);
    }
    return height;
}

// One wall of the preview: from the neighbour's height up to the cell's in steps of 10, one face on the side `side`.
void Walls(CMapCell* cell, const MapDelta& offset, uint8_t side, float base, int32_t x10, int32_t y10, bool clear, int32_t texture,
           int32_t keep) {
    MapDelta delta = offset;
    CMapCell* const neighbour = cell->GetNeighbouringCell(&delta, 1);
    if (neighbour == nullptr) return;
    const uint32_t id = static_cast<uint32_t>(neighbour->mCellId) - 1u;
    const float height = CellHeight(neighbour, static_cast<int32_t>(id & 0x7f), static_cast<int32_t>(id >> 7));
    if (base == height) return;
    for (int32_t level = ppc_fctiwz(static_cast<double>(height)); level < ppc_fctiwz(static_cast<double>(base)); level += 10) {
        if (clear) {
            gLLCCC210 = 0;
            gLLCCC214 = 0;
        }
        DC_AddSquareFaceToList(static_cast<float>(x10), 1.5f + static_cast<float>(level), static_cast<float>(y10), side, texture, 0, 1.0f, 0, 0,
                               keep, static_cast<DYN_Type>(0));
    }
}

// The track cell the cell stands for; a cross track (IsTrackX) is replaced by the world's track cell of its parent id (id 0 lands one before).
CTrackCell* ResolveTrack(CMapCell* cell) {
    CTrackCell* track = cell->GetTrackCellEquiv();
    if (track->IsTrackX() != 0) {
        const int32_t index = static_cast<int32_t>(track->mParentID) - 1;
        track = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + static_cast<intptr_t>(index) * 0x28);
    }
    return track;
}

}  // namespace

// 0x10097538. Answers 0 when the change failed (an illegal connection under 0x200, a refused ModifyType, the 0x81 refusal), else 1.
int32_t CMapCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    const int32_t full = static_cast<int32_t>(mode);
    const int32_t low = full & 0xff;
    const CELL_MODES lowMode = static_cast<CELL_MODES>(low);

    if ((full & 0x200) != 0) {
        LLCC_SystemGetMode();   // compared with 4, the answer unused
        return ConnectionType(lowMode, 0) == 1 ? 0 : 1;
    }

    if ((full & 0x100) != 0) {
        const uint32_t id = static_cast<uint32_t>(mCellId) - 1u;
        const int32_t x = static_cast<int32_t>(id & 0x7f);
        const int32_t y = static_cast<int32_t>(id >> 7);
        const int32_t keep = LLCC_SystemGetMode() != 4 ? 1 : 0;
        CTrackCell* const track = GetTrackCellEquiv();
        // A cross track's parent id is read into a local that is never used.
        track->IsTrackX();
        int32_t texture;
        int32_t rotation;
        if (low == 4 || low == 0x18 || (low == 0xb && a == 5) || (low == 0x10 && a == 5)) {
            texture = a;
            rotation = b;
        } else {
            texture = 0;
            rotation = 1;
        }
        const float base = CellHeight(this, x, y);
        const int32_t x10 = static_cast<int32_t>(static_cast<uint32_t>(x) * 10u);
        const int32_t y10 = static_cast<int32_t>(static_cast<uint32_t>(y) * 10u);
        int32_t connection = gLLCC47C8 != 0 ? 1 : ConnectionType(lowMode, 0);
        if (connection == 1) {
            ConnectionType(lowMode, 1);
            LLCM_SetIllegalPlacement(1);
            DC_AddSquareFaceToList(static_cast<float>(x10), 1.5f + base, static_cast<float>(y10), 0, 1, 1, 1.0f, 0, 0, 0, static_cast<DYN_Type>(0));
            Walls(this, gMapDeltaMinusY, 1, base, x10, y10, false, 1, 0);
            Walls(this, gMapDeltaPlusX, 4, base, x10, y10, false, 1, 0);
            Walls(this, gMapDeltaPlusY, 0x10, base, x10, y10, false, 1, 0);
            Walls(this, gMapDeltaMinusX, 0x40, base, x10, y10, false, 1, 0);
        } else {
            if (connection == 0) connection = texture;
            DC_AddSquareFaceToList(static_cast<float>(x10), 1.5f + base, static_cast<float>(y10), 0, connection, rotation, 1.0f, 0, 0, 0,
                                   static_cast<DYN_Type>(0));
            const int32_t wallTexture = connection == 2 ? 2 : 0;
            Walls(this, gMapDeltaMinusY, 1, base, x10, y10, true, wallTexture, keep);
            Walls(this, gMapDeltaPlusX, 4, base, x10, y10, true, wallTexture, keep);
            Walls(this, gMapDeltaPlusY, 0x10, base, x10, y10, true, wallTexture, keep);
            Walls(this, gMapDeltaMinusX, 0x40, base, x10, y10, true, wallTexture, keep);
        }
        return 1;
    }

    switch (low) {
    case 0x80:
        CalculateNeighbours(static_cast<CELL_MODES>(gModifyLastMode), a, b, true);
        return 1;
    case 0x81:
        SetHFieldTile();
        gModifyLastMode = 0;
        if (gValidTypeChangeLock != 0 && gModifyTypeRefused != 0) {
            gModifyTypeRefused = 0;
            return 0;
        }
        return 1;
    case 0x82:
        if (GetDirection() == 0) CalculateDirections(a, b);
        return 1;
    case 0x83: {
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> parent;
        parent.mIndex = gLLCCDB68;
        ModifyParentID(parent);
        return 1;
    }
    case 0x84:
        SetPylonIndex(static_cast<int16_t>(gModifyPylonIndex));
        gModifyPylonIndex = gModifyPylonIndex + 1;
        return 1;
    case 0x85: {
        CTrackCell* const track = ResolveTrack(this);
        if (track->IsTrack() == 0) return 1;
        if (static_cast<int32_t>(track->mType) == 0x19) return 1;
        if (track->IsCorner() != 0) return 1;
        if (track->IsCrossRoad() != 0) return 1;
        if (track->IsBridge() != 0) return 1;
        track->UnSetFlag(0xf);
        track->SetFlag(1);
        return 1;
    }
    case 0x86: {
        CTrackCell* const track = ResolveTrack(this);
        if (track->IsTrack() == 0) return 1;
        if (track->IsHoarding() != 0) return 1;
        if (track->mParentID == gModifyNoParent) return 1;
        const int32_t index = static_cast<int32_t>(track->mParentID) - 1;
        const CMapCell* const parent =
            reinterpret_cast<const CMapCell*>(reinterpret_cast<const char*>(gGameWorld) + 0x2d8 + static_cast<intptr_t>(index) * 0x44);
        const uint16_t thing = parent->GetObjectThingId().mIndex;
        if (thing == gModifyNoThing) return 1;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kModifyThingZero);
        CObject* const object = static_cast<CObject*>(gThingArray[thing].mThing);
        const bool running = object->mF28 != 0;
        if (LLCR_FixBridges(object, true, true, running) == 0) LLCR_FixBridgesReverse(object, false, true, running);
        return 1;
    }
    case 0x87:
        if (gLLCCDE0B8 > gLLCCDE0B4) {
            const uint32_t id = static_cast<uint32_t>(mCellId) - 1u;
            const int32_t x = static_cast<int32_t>(id & 0x7f);
            const int32_t y = static_cast<int32_t>(id >> 7);
            const int32_t saved4804 = gLLCC4804;
            const int32_t saved4800 = gLLCC4800;
            const int32_t systemMode = LLCC_SystemGetMode();
            if (IsPath() != 0 && systemMode != 3 && systemMode != 1) {
                Delete(0, 0);
            } else {
                LLCR_DeleteRide(x, y, 0);
            }
            gLLCC4804 = saved4804;
            gLLCC4800 = saved4800;
            LLCC_SystemSetModeNR(systemMode, false);
        }
        return 1;
    case 0x32: {
        Delete(a, b);
        int32_t paths = 0;
        CTrackCell* const track = ResolveTrack(this);
        if (track->IsTrack() == 0) return 1;
        if (track->IsBridge() == 0) return 1;
        CMapCell* const cell = track->GetMapCellEquiv();
        if (static_cast<int32_t>(cell->mType) == 1 || static_cast<int32_t>(cell->mType) == 3) paths++;
        const MapDelta* const around[3] = {&gMapDeltaPlusX, &gMapDeltaPlusY, &gMapDeltaPlusXPlusY};
        for (const MapDelta* offset : around) {
            MapDelta delta = *offset;
            const CMapCell* const neighbour = cell->GetNeighbouringCell(&delta, 1);
            if (neighbour != nullptr && (static_cast<int32_t>(neighbour->mType) == 1 || static_cast<int32_t>(neighbour->mType) == 3)) paths++;
        }
        if (paths != 0) return 1;
        switch (track->GetDirection()) {
        case 1:
        case 0x10:
            if (track->IsBridge() != 0) RemoveBridge(track, gMapDeltaPlusY, gMapDeltaMinusY);
            break;
        case 4:
        case 0x40:
            if (track->IsBridge() != 0) RemoveBridge(track, gMapDeltaMinusX, gMapDeltaPlusX);
            break;
        default:
            break;
        }
        return 1;
    }
    default:
        break;
    }

    gModifyTypeRefused = 0;
    if (gValidTypeChangeLock != 0 && full != 0x15 &&
        (static_cast<int32_t>(mType) == 3 || static_cast<int32_t>(mType) == 1)) {
        gModifyTypeRefused = 1;
    }
    if (full == 0x15 && (IsFlag(0x100) != 0 || IsRide() != 0 || IsEntrance() != 0 || IsExit() != 0)) gModifyTypeRefused = 1;
    if (ModifyTypeBody(mode) != 0) {
        gModifyLastMode = full;
        return 1;
    }
    return 0;
}
