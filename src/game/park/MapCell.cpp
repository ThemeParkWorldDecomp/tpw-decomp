// MapCell.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "analyser.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "HLSprite.h"
#include "HoardingsBalance.h"
#include "intrface.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "RegionEffectCell.h"
#include "RideDB.h"
#include "rsse.h"
#include "sprite_engine.h"
#include "tb_resource_display.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>


// 0x1009722c, 712 bytes: takes a bridge away from the cell and turns the two bridge cells beside it (a, b) to what they become without it.
// The two IsXYValid calls on the first block and the track end have their answers dropped (the Mac stores them in dead locals).
// A direction other than 1, 4, 0x10, 0x40 leaves the two flags (callee-saved r26, r25) as the caller had them: here 0.
void RemoveBridge(CTrackCell* cell, MapDelta a, MapDelta b) {
    CTrackCell* na = cell->GetNeighbouringCell(a, 2);
    CTrackCell* nb = cell->GetNeighbouringCell(b, 2);
    (void)CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY);
    (void)CMap::IsXYValid(gLLCCE034, gLLCCE038);
    uint16_t flag26 = 0;
    uint16_t flag25 = 0;
    switch (cell->GetDirection()) {
        case 1:
        case 4:
            flag26 = 2;
            flag25 = 8;
            break;
        case 0x10:
        case 0x40:
            flag26 = 8;
            flag25 = 2;
            break;
        default:
            break;
    }
    cell->UnSetFlag(0xf);
    if (na != nullptr && na->IsBridge() != 0) {
        if (na->IsFlag(flag26) != 0) {
            na->UnSetFlag(0xf);
            na->SetFlag(1);
            na->SetHFieldTile();
        } else if (na->IsFlag(4) != 0) {
            na->UnSetFlag(0xf);
            na->SetFlag(flag25);
            na->SetHFieldTile();
        }
    }
    if (nb != nullptr && nb->IsBridge() != 0) {
        if (nb->IsFlag(flag25) != 0) {
            nb->UnSetFlag(0xf);
            nb->SetFlag(1);
            nb->SetHFieldTile();
        } else if (nb->IsFlag(4) != 0) {
            nb->UnSetFlag(0xf);
            nb->SetFlag(flag26);
            nb->SetHFieldTile();
        }
    }
    cell->SetHFieldTile();
}

// CMapCell::Modify (0x10097538, 8360 bytes; unit v5637). The cell system's one entry for changing a map cell: bit 0x200 of the mode only
// asks whether the placement would connect, bit 0x100 draws the placement preview (the square and its walls down to the four neighbours),
// and the low byte otherwise picks a command (0x80 to 0x87, 0x32) or a mode to set through ModifyType.

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

// CMapCell::ModifyType 0x10099614 (740 bytes) and CMapCell::GetTile 0x1009bc54 (1108 bytes), main app (unit v5535).

// Globals nobody else defines. The two tile tables are the Mac's initialised data ({tile, rotation, neighbour mask}, read from the data
// section; unit v5L35). With only the first word of each, every path cell matched entry 0 of table B (mask 0) and drew the same piece.
uint16_t gNoObjectThingId = 0;     // 0x1029e212
uint8_t gTileRandomFlag = 0;       // 0x102e4830
int32_t gTileCountA = 11;          // 0x1029d8f4
int32_t gTileCountB = 0x31;        // 0x1029db44
CMapCellTileEntry gTileTableA[11] = {   // 0x1029d870 (the queue pieces)
    {0x00020002u, 0, 0x11, {}}, {0x00020002u, 90, 0x44, {}}, {0x00020001u, 0, 0x10, {}}, {0x00020001u, 90, 0x40, {}},
    {0x00020001u, 180, 0x01, {}}, {0x00020001u, 270, 0x04, {}}, {0x00020003u, 0, 0x14, {}}, {0x00020003u, 90, 0x50, {}},
    {0x00020003u, 180, 0x41, {}}, {0x00020003u, 270, 0x05, {}}, {0x00020000u, 0, 0x00, {}},
};
CMapCellTileEntry gTileTableB[49] = {   // 0x1029d8f8 (the path pieces; the masks include the diagonals)
    {0x0001000au, 0, 0x1f, {}}, {0x0001000au, 90, 0x7c, {}}, {0x0001000au, 180, 0xf1, {}}, {0x0001000au, 270, 0xc7, {}},
    {0x00010007u, 0, 0x1d, {}}, {0x00010007u, 90, 0x74, {}}, {0x00010007u, 180, 0xd1, {}}, {0x00010007u, 270, 0x47, {}},
    {0x0001000fu, 0, 0x71, {}}, {0x0001000fu, 90, 0xc5, {}}, {0x0001000fu, 180, 0x17, {}}, {0x0001000fu, 270, 0x5c, {}},
    {0x00010004u, 0, 0x15, {}}, {0x00010004u, 90, 0x54, {}}, {0x00010004u, 180, 0x51, {}}, {0x00010004u, 270, 0x45, {}},
    {0x00010002u, 0, 0x11, {}}, {0x00010002u, 90, 0x44, {}}, {0x0001000du, 0, 0xff, {}}, {0x0001000bu, 0, 0x7f, {}},
    {0x0001000bu, 90, 0xfd, {}}, {0x0001000bu, 180, 0xf7, {}}, {0x0001000bu, 270, 0xdf, {}}, {0x00010006u, 0, 0x7d, {}},
    {0x00010006u, 90, 0xf5, {}}, {0x00010006u, 180, 0xd7, {}}, {0x00010006u, 270, 0x5f, {}}, {0x0001000cu, 0, 0x77, {}},
    {0x0001000cu, 90, 0xdd, {}}, {0x0001000cu, 180, 0x77, {}}, {0x0001000cu, 270, 0xdd, {}}, {0x00010008u, 0, 0xd5, {}},
    {0x00010008u, 90, 0x57, {}}, {0x00010008u, 180, 0x5d, {}}, {0x00010008u, 270, 0x75, {}}, {0x00010005u, 0, 0x55, {}},
    {0x00010009u, 0, 0x1c, {}}, {0x00010009u, 90, 0x70, {}}, {0x00010009u, 180, 0xc1, {}}, {0x00010009u, 270, 0x07, {}},
    {0x00010003u, 0, 0x14, {}}, {0x00010003u, 90, 0x50, {}}, {0x00010003u, 180, 0x41, {}}, {0x00010003u, 270, 0x05, {}},
    {0x00010001u, 0, 0x10, {}}, {0x00010001u, 90, 0x40, {}}, {0x00010001u, 180, 0x01, {}}, {0x00010001u, 270, 0x04, {}},
    {0x00010000u, 0, 0x00, {}},
};

namespace {
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
constexpr const char* kThingZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023e42d
}  // namespace

// 0x10099614. 0 when ValidTypeChange refuses. The same mode again only counts (mOverlapCounter, a halfword). A change to the path (1) or queue (3)
// mode costs gold unless gTrackCostFree is set and the action recorder is idle; it is refused (0) when the balance falls short. The cell is stamped
// in the map; a queue turned back from a path... (type 3 to mode 1) tells the object of the parent cell to recalculate its queue.
// The header declares ModifyType void (unit v5211's declaration); the answer (r3) lives in this helper, which the test calls.
int32_t CMapCell_ModifyTypeImpl(CMapCell* self, CELL_MODES mode);
void CMapCell::ModifyType(CELL_MODES mode) { CMapCell_ModifyTypeImpl(this, mode); }

// 0x10099930, 16 bytes.
void CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) {
    mParentID = id.mIndex;
}

// CMapCell::CalculateNeighbours (unit v5632, main app 0x10099998, 5540 bytes; the inventory files it under MapDelta.cpp).

int32_t gCalcNeighboursPrevA = 0x80;
int32_t gCalcNeighboursPrevB = 0x80;
uint16_t gCalcNeighboursIdMinusY = 0;
uint16_t gCalcNeighboursIdPlusX = 0;
uint16_t gCalcNeighboursIdPlusY = 0;
uint16_t gCalcNeighboursIdMinusX = 0;
int32_t gCalcNeighboursJoinEntry = 0;

namespace {

// The neighbour at a compass offset (the original copies the offset's two words to the stack and passes their address, flag 1).
CMapCell* Neighbour(CMapCell* cell, const MapDelta& delta) {
    MapDelta copy = delta;
    return cell->GetNeighbouringCell(&copy, 1);
}

// Sound event 0x8b of the ui category at 10 times the cell's (id - 1) & 0x7f, twice (the original computes the same value for both).
void QueueSound(uint16_t cellId) {
    if (gActionRecorder.mRecording != 0) return;
    const int32_t v = static_cast<int32_t>((static_cast<int32_t>(cellId) - 1) & 0x7f);
    const float fv = static_cast<float>(v);
    const int32_t x = static_cast<int32_t>(10.0f * fv);
    const int32_t y = static_cast<int32_t>(10.0f * fv);
    SOUND_SendEvent(0, gSoundBanks[3], 0x8b, x, 0, y);
}

// One of the four straight neighbours of a mode 1 change (path cells): joins it when it has the mode, closes its side corners towards
// path cells that are not queues, then links type 10 and 9 cells by their direction and refreshes a queue that points back.
void Straight(CMapCell* self, CELL_MODES mode, bool flag, const MapDelta& delta, uint8_t mine, uint8_t theirs, const MapDelta& side1,
              uint8_t dir1, uint8_t add1, const MapDelta& side2, uint8_t dir2, uint8_t add2, uint8_t bit10, uint8_t bit9, uint8_t queueBit) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr) return;
    if (mode == n->mType) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
        CMapCell* m = Neighbour(n, side1);
        if (m != nullptr && static_cast<int32_t>(m->mType) == 1 && n->GetNeighbouringCell(dir1)->IsQueue() == 0) n->ModifyNeighbours(add1);
        m = Neighbour(n, side2);
        if (m != nullptr && static_cast<int32_t>(m->mType) == 1 && n->GetNeighbouringCell(dir2)->IsQueue() == 0) n->ModifyNeighbours(add2);
        if (flag) n->SetHFieldTile();
    }
    if (static_cast<int32_t>(n->mType) == 10 && (n->GetDirection() & bit10) != 0) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
        if (flag) n->SetHFieldTile();
    }
    if (static_cast<int32_t>(n->mType) == 9) {
        if ((n->GetDirection() & bit9) != 0) {
            self->ModifyNeighbours(mine);
            n->ModifyNeighbours(theirs);
            if (flag) n->SetHFieldTile();
        }
    } else if (n->IsQueue() != 0 && (n->GetNeighbours() & queueBit) != 0 && flag) {
        n->SetHFieldTile();
        QueueSound(self->mCellId);
    }
}

// One of the four diagonal neighbours: joined when it has the mode and both straight cells beside it are paths (mode 1). The two straight
// cells are read without a NULL check (the original's).
void Diagonal(CMapCell* self, CELL_MODES mode, bool flag, const MapDelta& delta, const MapDelta& check1, const MapDelta& check2, uint8_t mine,
              uint8_t theirs) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr || mode != n->mType) return;
    if (static_cast<int32_t>(Neighbour(self, check1)->mType) != 1) return;
    if (static_cast<int32_t>(Neighbour(self, check2)->mType) != 1) return;
    self->ModifyNeighbours(mine);
    n->ModifyNeighbours(theirs);
    if (flag) n->SetHFieldTile();
}

// Path B's first part: the straight neighbour joins when it is the current object's entry cell and faces this cell.
void JoinEntry(CMapCell* self, const MapDelta& delta, uint8_t direction, const CMapCell* entry, uint8_t mine, uint8_t theirs) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr) return;
    if (n->GetDirection() != direction) return;
    if (n != entry) return;
    self->ModifyNeighbours(mine);
    n->ModifyNeighbours(theirs);
}

// Path B's second part: the straight neighbour of the mode joins when it has fewer than two neighbours and its parent is the current
// cell (gLLCCDB68) or the side's halfword; a mode 1 cell then loses the two diagonals beside it.
void JoinEnd(CMapCell* self, CELL_MODES mode, const MapDelta& delta, uint16_t other, uint8_t mine, uint8_t theirs, uint8_t remove1,
             uint8_t remove2) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr || mode != n->mType) return;
    if (n->NumberOfNeighbours() < 2 && (n->mParentID == gLLCCDB68 || n->mParentID == other)) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
    }
    if (static_cast<int32_t>(self->mType) == 1) {
        self->RemoveNeighbour(remove1);
        self->RemoveNeighbour(remove2);
    }
}

}  // namespace

// 0x10099998. Mode 1 (paths) links the cell with its eight neighbours of the same mode (a mode 3 cell becomes 1 and is time stamped first),
// then drops the diagonals beside straight queue neighbours. Modes 0 and 3 join the current object's entry cell and the straight neighbour
// a / b (or the kept pair) points to. Every mode keeps a and b for the next call.
void CMapCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag) {
    const int32_t m = static_cast<int32_t>(mode);
    if (m == 1) {
        if (static_cast<int32_t>(mType) == 3) {
            mType = static_cast<CELL_MODES>(1);
            CMap* const map = gGameWorld->GetMap();
            const uint32_t stamp = map->MakeNewTimeStamp();
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
            id.mIndex = mCellId;
            gGameWorld->GetMap()->SetTimeStamp(stamp, id);
        }
        Straight(this, mode, flag, gMapDeltaMinusY, 1, 0x10, gMapDeltaPlusXPlusY, 4, 8, gMapDeltaMinusXPlusY, 0x40, 0x20, 0x10, 1, 0x10);
        Diagonal(this, mode, flag, gMapDeltaPlusXMinusY, gMapDeltaMinusY, gMapDeltaPlusX, 2, 0x20);
        Straight(this, mode, flag, gMapDeltaPlusX, 4, 0x40, gMapDeltaMinusXMinusY, 1, 0x80, gMapDeltaMinusXPlusY, 0x10, 0x20, 0x40, 4, 0x40);
        Diagonal(this, mode, flag, gMapDeltaPlusXPlusY, gMapDeltaPlusY, gMapDeltaPlusX, 8, 0x80);
        Straight(this, mode, flag, gMapDeltaPlusY, 0x10, 1, gMapDeltaPlusXMinusY, 4, 2, gMapDeltaMinusXMinusY, 0x40, 0x80, 1, 0x10, 1);
        Diagonal(this, mode, flag, gMapDeltaMinusXPlusY, gMapDeltaPlusY, gMapDeltaMinusX, 0x20, 2);
        Straight(this, mode, flag, gMapDeltaMinusX, 0x40, 4, gMapDeltaPlusXMinusY, 1, 2, gMapDeltaPlusXPlusY, 0x10, 8, 4, 0x40, 4);
        Diagonal(this, mode, flag, gMapDeltaMinusXMinusY, gMapDeltaMinusY, gMapDeltaMinusX, 0x80, 8);

        uint8_t dir = 1;
        do {
            if ((dir & GetNeighbours()) != 0) {
                CMapCell* const cell = GetNeighbouringCell(dir);
                if (cell != nullptr && cell->IsQueue() != 0) {
                    switch (dir) {
                    case 1:
                        RemoveNeighbour(2);
                        RemoveNeighbour(0x80);
                        break;
                    case 4:
                        RemoveNeighbour(2);
                        RemoveNeighbour(8);
                        break;
                    case 0x10:
                        RemoveNeighbour(8);
                        RemoveNeighbour(0x20);
                        break;
                    case 0x40:
                        RemoveNeighbour(0x80);
                        RemoveNeighbour(0x20);
                        break;
                    default:
                        break;
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
    } else if (m == 0 || m == 3) {
        // The current object's entry cell (the Mac reads the object through the 32-bit word gLLCCDB6C; the host keeps the pointer whole).
        const uint16_t entryId = gLLCMCurrentObject->mF32;
        const CMapCell* const entry = reinterpret_cast<const CMapCell*>(reinterpret_cast<const char*>(gGameWorld) + 0x2d8 +
                                                                        (static_cast<int32_t>(entryId) - 1) * 0x44);
        int32_t x;
        int32_t y;
        if (gCalcNeighboursJoinEntry != 0) {
            JoinEntry(this, gMapDeltaMinusY, 0x10, entry, 1, 0x10);
            JoinEntry(this, gMapDeltaPlusX, 0x40, entry, 4, 0x40);
            JoinEntry(this, gMapDeltaPlusY, 1, entry, 0x10, 1);
            JoinEntry(this, gMapDeltaMinusX, 4, entry, 0x40, 4);
            if (gCalcNeighboursPrevA == 0x80 || gCalcNeighboursPrevB == 0x80) {
                x = 0;
                y = 0;
            } else {
                x = gCalcNeighboursPrevA;
                y = gCalcNeighboursPrevB;
            }
        } else {
            x = a;
            y = b;
        }
        if (y == 1) JoinEnd(this, mode, gMapDeltaMinusY, gCalcNeighboursIdMinusY, 1, 0x10, 2, 0x80);
        if (x == -1) JoinEnd(this, mode, gMapDeltaPlusX, gCalcNeighboursIdPlusX, 4, 0x40, 2, 8);
        if (y == -1) JoinEnd(this, mode, gMapDeltaPlusY, gCalcNeighboursIdPlusY, 0x10, 1, 8, 0x20);
        if (x == 1) JoinEnd(this, mode, gMapDeltaMinusX, gCalcNeighboursIdMinusX, 0x40, 4, 0x80, 0x20);
    }
    gCalcNeighboursPrevA = a;
    gCalcNeighboursPrevB = b;
}

// CMapCell::IsHoarding 0x1009ca9c, IsTrack 0x1009c96c, RefreshHFieldTile 0x1009cf64, ValidTypeChange 0x1009b054, CalculateDirections 0x1009af80
// (unit v5270; main app).

// 0x1029e224: the parent id IsTrack answers 0 for (initial value 0).
uint16_t gIsTrackNoParent;
// 0x102e4834: ValidTypeChange refuses mode 3 on a mode 1 cell while it is nonzero (initial value 0).
int32_t gValidTypeChangeLock;

namespace {

// The track cell a parent id names: the track table lies in the world at +0x1102d8, 0x28 bytes each, id 1 first (id 0 lands one cell before).
const CTrackCell* TrackCellOfParent(uint16_t id) {
    const uint8_t* world = reinterpret_cast<const uint8_t*>(gGameWorld);
    const int32_t offset = (static_cast<int32_t>(id) - 1) * 0x28;
    return reinterpret_cast<const CTrackCell*>(world + 0x2d8 + static_cast<int64_t>(offset) + 0x110000);
}

}  // namespace

// 0x1009af80, 160 bytes: sets the direction bits for a step (-1/+1 in the first argument: 4 / 0x40; in the second: 0x10 / 1).
void CMapCell::CalculateDirections(int32_t a, int32_t b) {
    if (a == -1) ModifyDirection(4);
    if (a == 1) ModifyDirection(0x40);
    if (b == -1) ModifyDirection(0x10);
    if (b == 1) ModifyDirection(1);
}
// 0x1009b054, 200 bytes: whether the cell may change to the mode.
int32_t CMapCell::ValidTypeChange(CELL_MODES mode) {
    const int32_t m = static_cast<int32_t>(mode);
    const int32_t t = static_cast<int32_t>(mType);
    if (m == 0) return 1;
    if (m == 4 && t == 4) return 0;
    if (m == 0x15 && t == 0x15) return 1;
    if (m == 1 && t == 3) return 1;
    if (m == t || t == 0) return 1;
    if (m == 4 && t == 1) return 1;
    if (m == 3 && t == 1 && gValidTypeChangeLock == 0) return 1;
    return 0;
}

namespace {
// 0x1023e42d: the assertion text of the thing table's entry 0.
constexpr const char* kThingZeroText = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
}  // namespace

// 0x1009b158 (2,524 bytes). Answers whether placing `mode` on this cell is legal: 0 for fine, 1 for an illegal placement, other values (2, 8, 0xb, 0xd)
// for the special cases. The globals gLLCCC200 .. gLLCCC214 are the "why" flags the cell system reads afterwards. The mode 0x18 counts as 1.
int32_t CMapCell::ConnectionType(CELL_MODES modeIn, int32_t flag) {
    int32_t mode = static_cast<int32_t>(modeIn);
    int32_t cost = 0;
    if (mode == 0x1f) return 2;
    if (mode == 0x3a) return 0xd;
    if (IsFlag(0x20) != 0) {
        if (mode == 4 || static_cast<uint32_t>(mode - 9) <= 1u) return 1;
        if (mode == 0x18 && static_cast<int32_t>(mType) == 1) return 0;
    }
    if (mode == 0x18) mode = 1;
    if (mode == 0x36) return gLLCCC214 != 0 ? 1 : 0;
    int32_t type = static_cast<int32_t>(mType);
    if (type == 4 || type == 9 || type == 10) {
        CMapCell* const object = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 +
                                                              (static_cast<int32_t>(mParentID) - 1) * 0x44);
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId = object->GetObjectThingId();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZeroText);
        const uint8_t* const thing = static_cast<const uint8_t*>(gThingArray[thingId.mIndex].mThing);
        if (thing[2] == 3) {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> objectType;
            objectType.mIndex = *reinterpret_cast<const uint16_t*>(thing + 0xe);
            RideInfo* const ride = gRideDatabase.GetRideInfo(objectType);
            gLLCCDE0B4 = ride->Value(kRideSlot_OverwritePriority).mInteger;   // the Mac's +0x54, by name: the host's record is laid out wider
        }
    }
    if (gLLCCC214 == 0 && mode == 0x15 && static_cast<int32_t>(mType) == 0x15 && IsFlag(0x100) != 0 && mParentID == gLLCCDB68) {
        gLLCCC200 = 1;
        return 0xb;
    }
    if (IsFlag(0x40) != 0) return 1;
    type = static_cast<int32_t>(mType);
    if (type == 4 || type == 9 || type == 10) {
        if (gLLCCDE0B8 > gLLCCDE0B4 && mode != 0x15 && mode != 0x16) {
            if (gLLCCC210 == 0 && gLLCCC214 == 0) return 0;
            gLLCCC214 = 1;
            return 1;
        }
        gLLCCC214 = 1;
        return 1;
    }
    if (mode == 1) cost = LLCC_SystemGetPathCost();
    if (mode == 3) cost = LLCC_SystemGetQueueCost();
    CTrackCell* track = GetTrackCellEquiv();
    if (track->IsTrackX() != 0) {
        track = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 +
                                              (static_cast<int32_t>(track->mParentID) - 1) * 0x28 + 0x110000);
    }
    if (mode == 0xb && track->IsTrack() != 0 && gLLCCE0DC == 0) {
        gLLCCC214 = 1;
        return 1;
    }
    if (mode == 0x10 && track->IsTrack() != 0 && gLLCCE0DC == 0) {
        gLLCCC214 = 1;
        return 1;
    }
    if (gLLCCC210 != 0) gLLCCC214 = 1;
    if (flag == 0 && gLLCCC214 != 0) return 1;
    if (static_cast<int32_t>(track->mType) == 0x19) {
        gLLCCC214 = 1;
        return 1;
    }
    if (ValidTypeChange(static_cast<CELL_MODES>(mode)) != 0) {
        const int32_t current = static_cast<int32_t>(mType);
        if (current != mode && gTrackCostFree == 0 && !(mode == 3 && current == 1)) LLCC_SystemAddToSessionCost(cost);
    }
    if (gActionRecorder.mRecording == 0 && LLCC_SystemGetSessionCost() != 0) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        if (LLCC_SystemGetSessionCost() > balance) {
            gLLCCC214 = 1;
            LLCM_SetIllegalPlacement(1);
            gLLCCC204 = 1;
            return 1;
        }
    }
    if (mode == 0x36) return 0;
    if (gLLCCE0DC != 0 && (mode == 0xb || mode == 0x10)) {
        if (IsFlag(0x10) == 0 && track->IsTrack() != 0 && track->IsCorner() == 0 && track->IsCrossRoad() == 0 &&
            track->mParentID == gLLCCDB68 && static_cast<int32_t>(mType) != 3)
            return 0;
        return 1;
    }
    if (mode == 0x15 && IsTrack() != 0) return 1;
    if (mode == 0x16 && IsTrack() != 0) return 1;
    if (mode == 0x14) {
        if (static_cast<int32_t>(mType) == 3) return mParentID == gLLCCDB68 ? 0 : 1;
        return 1;
    }
    if (mode == 3 && static_cast<int32_t>(mType) == 3) {
        if (NumberOfNeighbours() == 2 && gLLCCDB40 == 0) {
            gLLCCC214 = 1;
            return 1;
        }
        if (mParentID != gLLCCDB68) {
            gLLCCC214 = 1;
            return 1;
        }
        if (gValidTypeChangeLock != 0) {
            gLLCCC200 = 1;
            return 0xb;
        }
        return 0;
    }
    type = static_cast<int32_t>(mType);
    if (mode == type && ((gCalcNeighboursJoinEntry != 0 && gLLCCDB60 != 0) || gValidTypeChangeLock != 0)) {
        if (type == 3 && NumberOfNeighbours() == 2) return 1;
        gLLCCC200 = 1;
        return 0xb;
    }
    if (static_cast<uint32_t>(mode - 0x15) <= 1u && IsFlag(0x20) == 0 && static_cast<int32_t>(mType) == 1) return 0;
    if (static_cast<uint32_t>(mode - 0x15) <= 1u && static_cast<int32_t>(mType) == 0x15 && mParentID == gLLCCDB68) return 0;
    if (static_cast<int32_t>(mType) == 0x15) {
        gLLCCC210 = 1;
        return 1;
    }
    if (mode == 4 && track->IsTrack() != 0) return gLLCCE0DC == 0 ? 1 : 0;
    if (track->IsCorner() != 0 || track->IsCrossRoad() != 0) {
        gLLCCC210 = 1;
        return 1;
    }
    if (mode == 1 && static_cast<int32_t>(mType) == 1) return 0;
    if (mode == 0x15 && static_cast<int32_t>(mType) == 0x15) return 0;
    if (mode == 4 && static_cast<int32_t>(mType) == 4) return 1;
    if (mode == static_cast<int32_t>(mType) && mParentID == gLLCCDB4C->mCellId) return 0;
    const int32_t now = static_cast<int32_t>(mType);
    if (now == 0) return 0;
    if (mode == 3 && now == 1) {
        if (gValidTypeChangeLock != 0 && gLLCCC214 == 0) {
            gLLCCC208 = 1;
            gLLCCC210 = 1;
            return 8;
        }
        if (IsFlag(0x20) == 0) return 0;
        gLLCCC210 = 1;
        return 1;
    }
    if (mode == 3 && now == 4) {
        gLLCCC210 = 1;
        return 1;
    }
    if (mode == 1 && (now == 4 || now == 9)) {
        gLLCCC210 = 1;
        return 1;
    }
    if (mode == 1 && now == 3) {
        if (NumberOfNeighbours() == 1) {
            CMapCell* const neighbour = GetNeighbouringCell(GetNeighbours());
            if (static_cast<int32_t>(neighbour->mType) == 3) {
                gLLCCC208 = 1;
                return 8;
            }
            gLLCCC214 = 1;
            return 1;
        }
        gLLCCC214 = 1;
        return 1;
    }
    if (mode == 4 && now == 1) return 0;
    if (mode == 0xb && now == 1) return 0;
    if (mode == 0x10 && now == 1) return 0;
    return 1;
}

// CMapCell::IsFlag (main app, SimTheme Park 2.04b3, 0x1009bb70, 32 bytes; unit w201): whether any bit of `flag` (its low halfword) is set in the cell's
// flag word.

int32_t CMapCell::IsFlag(uint16_t flag) const { return (mFlags & flag) != 0 ? 1 : 0; }

// 0x1009bbb8, 8 bytes.
void CMapCell::SetFlag(uint16_t flag) {
    mFlags = flag;
}

// 0x1009bbe8, 8 bytes.
uint16_t CMapCell::GetFlag() {
    return mFlags;
}

// CMapCell's flag clearing and two terrain tests (main app, SimTheme Park 2.04b3; unit w206): UnSetFlag 0x1009bc18 (16 bytes), IsWater 0x1009c408
// (28 bytes) and IsFreeform 0x1009c59c (36 bytes). The tests answer a word, 1 or 0, from the cell's mode alone.

// 0x1009bc18, 16 bytes: clears the bits of flag in the flag word (only the low halfword is stored).
void CMapCell::UnSetFlag(uint16_t flag) { mFlags = static_cast<uint16_t>(mFlags & ~flag); }

int32_t CMapCell_ModifyTypeImpl(CMapCell* self, CELL_MODES mode) {
    return self->ModifyTypeBody(mode);
}

int32_t CMapCell::ModifyTypeBody(CELL_MODES mode) {
    const int32_t m = static_cast<int32_t>(mode);
    bool fromQueue = false;
    uint32_t cost = 0;
    if (ValidTypeChange(mode) == 0) return 0;
    if (m != 0 && static_cast<int32_t>(mType) == m) {
        mOverlapCounter = static_cast<int16_t>(mOverlapCounter + 1);
        return 1;
    }
    if (m == 1 && static_cast<int32_t>(mType) == 3) fromQueue = true;
    if (m == 1 && gTrackCostFree == 0) cost = static_cast<uint32_t>(LLCC_SystemGetPathCost());
    if (m == 3 && gTrackCostFree == 0) cost = static_cast<uint32_t>(LLCC_SystemGetQueueCost());
    if (m == 3 && static_cast<int32_t>(mType) == 1) {
        gLLCC47E4 = 1;
        Delete(0, 0);
        gLLCC47E4 = 0;
    }
    uint8_t affordable = 1;
    if (gActionRecorder.mRecording == 0) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        const int32_t left = static_cast<int32_t>(static_cast<uint32_t>(balance) - cost);
        affordable = left >= 0 ? 1 : 0;
    }
    if ((m == 1 || m == 3) && affordable == 0) return 0;
    mType = mode;
    CMap* const map = gGameWorld->GetMap();
    const uint32_t stamp = map->MakeNewTimeStamp();
    MapId self;
    self.mIndex = mCellId;
    map->SetTimeStamp(stamp, self);
    if (fromQueue) {
        CMapCell* const parent = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) +
                                                              (static_cast<int32_t>(mParentID) - 1) * 0x44);
        const int32_t parentType = static_cast<int32_t>(parent->mType);
        if (parentType == 4 || parentType == 9 || parentType == 10) {
            const ThingId thing = parent->GetObjectThingId();
            if (thing.mIndex != gNoObjectThingId) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZero);
                static_cast<CObject*>(gThingArray[thing.mIndex].mThing)->RecalculateBackOfQueue();
            }
        }
    }
    if (m == 1 && gTrackCostFree == 0) gGameWorld->GetBankAccount()->MakeWithdrawal(cost);
    if (m == 3) {
        const int32_t d = 1 - static_cast<int32_t>(mType);
        if (d != 0 && gTrackCostFree == 0) gGameWorld->GetBankAccount()->MakeWithdrawal(cost);
    }
    return 1;
}

// 0x1009bc54. The tile word (kind << 16 | index) for the cell's mode and neighbour byte; *rotation gets the table's rotation (plus 0xb4 for a mode 3
// straight that does not run along 0x40 / 0x10). The modes 2, 7, 0x1e, 4, 9, 10 and 0x15 (by absolute value) answer 8 with rotation 0.
uint32_t CMapCell::GetTile(int32_t* rotation) {
    const uint8_t neighbours = GetNeighbours();
    const int32_t type = static_cast<int32_t>(mType);
    const uint8_t direction = GetDirection();
    *rotation = 0;
    int32_t extra = 0;
    const int32_t a = std::abs(type);
    if (a == 2 || a == 7 || a == 0x1e || a == 4 || a == 9 || a == 10 || a == 0x15) return 8;
    const CMapCellTileEntry* table;
    int32_t count;
    if (a == 2 || a < 2 || a > 3) {
        count = gTileCountB;
        table = gTileTableB;
    } else {
        count = gTileCountA;
        table = gTileTableA;
        if (IsStraight() != 0) extra = (direction == 0x40 || direction == 0x10) ? 0 : 0xb4;
    }
    uint32_t tile = 0xffffffffu;
    bool found = false;
    for (int32_t i = 0; i < count && !found; ++i) {
        if (neighbours == table[i].mMask) {
            found = true;
            tile = table[i].mTile;
            *rotation = extra + table[i].mRotation;
        }
    }
    if (tile == 0xffffffffu) {
        found = false;
        for (int32_t i = 0; i < count && !found; ++i) {
            const uint8_t mask = table[i].mMask;
            if (mask == (neighbours & mask)) {
                found = true;
                tile = table[i].mTile;
                *rotation = extra + table[i].mRotation;
            }
        }
    }
    if (type == 3 && IsCorner() != 0) {
        const uint8_t d = GetDirection();
        const uint8_t n = GetNeighbours();
        if ((d == 0x40 && n == 0x50) || (d == 0x10 && n == 0x14) || (d == 1 && n == 0x41) || (d == 4 && n == 5)) tile = tile + 1;
    }
    if (gTileRandomFlag != 0 && (tile & 0xffff0000u) == 0x10000u) {
        if ((tile & 0xffffu) == 2) tile = tile + 0x11;
        if ((tile & 0xffffu) == 10) tile = tile + 10;
    }
    const int32_t r = rand();
    gTileRandomFlag = static_cast<uint8_t>(r - (r / 2) * 2);
    if ((tile & 0xffff0000u) != 0x20000u) return tile;
    uint8_t dir = 1;
    do {
        if ((dir & GetNeighbours()) != 0) {
            CMapCell* const other = GetNeighbouringCell(dir);
            if (other != nullptr && other->IsPath() != 0) {
                const uint8_t opposite = OPPOSITE(dir);
                if ((other->GetNeighbours() & opposite) != 0) {
                    int32_t bridge;
                    if (IsTrackX() != 0) {
                        CTrackCell* const equiv = GetTrackCellEquiv();
                        const uint16_t id = equiv->mParentID;
                        bridge = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 +
                                                               (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000)->IsBridge();
                    } else {
                        bridge = GetTrackCellEquiv()->IsBridge();
                    }
                    if (bridge == 0) tile = tile + 3;
                }
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);
    return tile;
}

namespace {

int32_t Abs(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// The cell the listing computes: world + 0x2d8 + (u16(x + (y << 7) + 1) - 1) * 0x44.
CMapCell* LineCell(int32_t x, int32_t y) {
    uint16_t index = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
    return &gGameWorld->mMapCells[static_cast<int32_t>(index) - 1];
}

}  // namespace

// 0x1009c0d0 (768 bytes): walks from (x, y) to the end point along the longer axis (the x axis when |dx| > |dy|), Modify on each valid
// cell with the step on that axis; the last cell is done with gValidTypeChangeLock set. Answers 0 as soon as a Modify refuses.
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    LLCC_DrawLineBody(mode, x, y, endX, endY);
}

int32_t LLCC_DrawLineBody(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    gCalcNeighboursJoinEntry = 1;
    gValidTypeChangeLock = 0;
    int32_t dy = Abs(*endY - y);
    int32_t dx = Abs(*endX - x);
    if (dx > dy) {
        int32_t step = (x > *endX) ? -1 : 1;
        for (; x != *endX; x += step) {
            if (CMap::IsXYValid(x, y)) {
                if (LineCell(x, y)->Modify(mode, step, 0) == 0) {
                    gCalcNeighboursJoinEntry = 0;
                    return 0;
                }
                gCalcNeighboursJoinEntry = 0;
            }
        }
        gValidTypeChangeLock = 1;
        if (CMap::IsXYValid(x, y)) {
            if (LineCell(x, y)->Modify(mode, step, 0) == 0) {
                gCalcNeighboursJoinEntry = 0;
                gValidTypeChangeLock = 0;
                return 0;
            }
            gCalcNeighboursJoinEntry = 0;
            gValidTypeChangeLock = 0;
        }
    } else {
        int32_t step = (y > *endY) ? -1 : 1;
        for (; y != *endY; y += step) {
            if (CMap::IsXYValid(x, y)) {
                if (LineCell(x, y)->Modify(mode, 0, step) == 0) {
                    gCalcNeighboursJoinEntry = 0;
                    return 0;
                }
                gCalcNeighboursJoinEntry = 0;
            }
        }
        gValidTypeChangeLock = 1;
        if (CMap::IsXYValid(x, y)) {
            if (LineCell(x, y)->Modify(mode, 0, step) == 0) {
                gCalcNeighboursJoinEntry = 0;
                gValidTypeChangeLock = 0;
                return 0;
            }
            gCalcNeighboursJoinEntry = 0;
            gValidTypeChangeLock = 0;
        }
    }
    return 1;
}

// 0x1009c408, 28 bytes: the cell's mode is 2.
int32_t CMapCell::IsWater() const { return mType == 2 ? 1 : 0; }

// 0x1009c44c, 28 bytes: the mode is 1.
int32_t CMapCell::IsPath() const {
    return static_cast<int32_t>(mType) == 1 ? 1 : 0;
}

// Small predicates and accessors of CMapCell and CTrackCell (unit v51060).

// 0x1009c490, 36 bytes: the mode is 3 or 9.
int32_t CMapCell::IsQueue() const {
    const int32_t t = static_cast<int32_t>(mType);
    if (t == 3 || t == 9) return 1;
    return 0;
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

// 0x1009c59c, 36 bytes: the cell's mode is 7, else 0x1e.
int32_t CMapCell::IsFreeform() const {
    if (mType == 7) return 1;
    return mType == 0x1e ? 1 : 0;
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

// CMapCell::IsCorner 0x1009c7fc (main app, SimTheme Park 2.04b3; lobby.cpp on the Mac, the inventory's file for this range; unit w204): the
// twin of CTrackCell::IsCorner (src/TrackCell_flags.cpp).

// 0x1009c7fc, 156 bytes: the neighbour byte (GetNeighbours is read first) when exactly two of the four horizontal and vertical neighbours are set and
// they are at a right angle; the tests run in the listing's order: 1 and 4, 1 and 0x40, 0x10 and 4, 0x10 and 0x40. Else 0.
uint8_t CMapCell::IsCorner() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 1) != 0 && (neighbours & 0x40) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 4) != 0) return neighbours;
    if ((neighbours & 0x10) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// 0x1009c8c4, 124 bytes: the neighbour byte when exactly two in a line, else 0.
uint8_t CMapCell::IsStraight() const {
    const uint8_t neighbours = GetNeighbours();
    if (NumberOfHVNeighbours() != 2) return 0;
    if ((neighbours & 1) != 0 && (neighbours & 0x10) != 0) return neighbours;
    if ((neighbours & 4) != 0 && (neighbours & 0x40) != 0) return neighbours;
    return 0;
}

// 0x1009c96c, 176 bytes: as IsHoarding, but a track X cell whose parent id is the sentinel answers 0 and CTrackCell::IsTrack is asked.
int32_t CMapCell::IsTrack() const {
    const CTrackCell* cell = GetTrackCellEquiv();
    if (cell->IsTrackX()) {
        if (cell->mParentID == gIsTrackNoParent) return 0;
        cell = TrackCellOfParent(cell->mParentID);
    }
    return cell->IsTrack();
}

// 0x1009ca44, 44 bytes: the track cell's answer for the cell's track twin.
int32_t CMapCell::IsTrackX() const {
    return GetTrackCellEquiv()->IsTrackX();
}

// 0x1009ca9c, 128 bytes: the track cell the cell stands for; when that is a "track X" cell, the one its parent id names instead (no sentinel
// test here). Answers CTrackCell::IsHoarding of it.
int32_t CMapCell::IsHoarding() const {
    const CTrackCell* cell = GetTrackCellEquiv();
    if (cell->IsTrackX()) cell = TrackCellOfParent(cell->mParentID);
    return cell->IsHoarding();
}

// The map reset (main app of SimTheme Park 2.04b3; the inventory files LLCC_ResetMap under LLCellSystem.cpp, HFieldCellStatus under
// TPRide.cpp, RemoveNeighbour and ModifyDirection under LLCellMouse.cpp and the cell resets under unrelated names; unit w112c):
// LLCC_ResetMap puts every cell of the map and of the track layer back to its start state (the cell's place comes from its id, the
// start state from the height field and, for the map cells, the attribute map).

// 0x1009cb48 (580 bytes). The cell's place comes from its id (32 bit arithmetic, so id 0 gives x = 0x7f and y = 0x1ffffff). The
// stores come in the listing's order: the callees are stand-ins that look at the cell. The attribute byte is read after the first
// stores and before HFieldCellStatus. When that answers 0 (the cell is outside the height field or its quad is flagged) the mode is 2
// (attribute 2), 0x1e (attribute 0x80) or 7, and the tile is set when `full`. Otherwise 5, 2 or 0x1e are stored and at once
// overwritten (dead stores) by the attribute 8 test: mode 1 with CalculateNeighbours and the flags set to 0x20, or mode 0.
void CMapCell::Reset(bool full) {
    const uint32_t id = mCellId;
    const int32_t x = static_cast<int32_t>((id - 1u) & 0x7fu);
    const int32_t y = static_cast<int32_t>((id - 1u) >> 7);
    RemoveNeighbour(0xff);
    ModifyDirection(0);
    mParentID = 0;
    mFlags = 0;
    mTileData[0] = 0;
    mTileData[1] = 0;
    mTileData[2] = 0;
    const uint8_t attribute = mStatusFlags;
    if (HFieldCellStatus(x, y) != 0) {
        if ((attribute & 1) != 0) {
            mType = static_cast<CELL_MODES>(5);
        } else if ((attribute & 2) != 0) {
            mType = static_cast<CELL_MODES>(2);
        } else if ((attribute & 0x80) != 0) {
            mType = static_cast<CELL_MODES>(0x1e);
        }
        if ((attribute & 8) != 0) {
            mType = static_cast<CELL_MODES>(1);
            CalculateNeighbours(mType, 0, 0, full);
            mFlags = 0x20;
        } else {
            mType = static_cast<CELL_MODES>(0);
        }
        if ((mFlags & 0x20) == 0) {
            mFlags = static_cast<uint16_t>(mFlags | 0x40);
            if (full) SetHFieldTile();
        }
    } else {
        if ((attribute & 2) != 0) {
            mType = static_cast<CELL_MODES>(2);
        } else if ((attribute & 0x80) != 0) {
            mType = static_cast<CELL_MODES>(0x1e);
        } else {
            mType = static_cast<CELL_MODES>(7);
        }
        if ((mFlags & 0x20) == 0) mFlags = static_cast<uint16_t>(mFlags | 0x40);
        if (full) SetHFieldTile();
    }
    if ((attribute & 0x10) != 0) mFlags = static_cast<uint16_t>(mFlags | 0x80);
    if ((attribute & 4) != 0) mFlags = static_cast<uint16_t>(mFlags | 0x400);
    mMeshInstance = 0;
    mOverlapCounter = 0;
}

int32_t HFieldCellStatus(int32_t x, int32_t y);

// CMapCell::SetHFieldTile (main app, SimTheme Park 2.04b3, MapCell.cpp; unit w139), 0x1009cdb4, 384 bytes.
//
// A cell whose mode is 0 or less, or 5, gets the blank tile 0x37 with rotation 0; any other mode asks GetTile for its tile word and
// rotation. The tile word is split into (word >> 16 arithmetic, word & 0xffff, rotation) in the cell system's scratch tile gLLCC4824, and
// the cell's mTileData receives a copy. The cell's place comes from its id (32 bit arithmetic: id 0 gives x = 0x7f, y = 0x1ffffff), read
// after the stores. A queue tile (kind 2) replaces the cell's mesh by a new queue mesh (the old one destroyed first) and then repaints the
// height field with the plain tile {0, 8, 0}; any other tile only drops the old mesh and repaints with the cell's own tile data.

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

// 0x1009cf64, 228 bytes: clears the mesh; the height-field tile is set at the cell's place (x = (id - 1) & 0x7f, y = (id - 1) >> 7, 32 bits
// so id 0 gives 0x1ffffff). A tile of kind 2 first gets a queue mesh and then the plain tile {0, 8, 0}.
void CMapCell::RefreshHFieldTile() {
    const uint32_t id = mCellId;
    mMeshInstance = 0;
    const int32_t x = static_cast<int32_t>((id - 1) & 0x7f);
    const int32_t y = static_cast<int32_t>((id - 1) >> 7);
    if (static_cast<int32_t>(mTileData[0]) == 2) {
        CELL_IndexType index = {mTileData[0], mTileData[1], mTileData[2]};
        mMeshInstance = LLCCM_CreateQMesh(x, y, index, 1);
        CELL_IndexType plain = {0, 8, 0};
        SingleHFieldTextureChange(x, y, &plain);
    } else {
        SingleHFieldTextureChange(x, y, reinterpret_cast<const CELL_IndexType*>(&mTileData[0]));
    }
}

// 0x1009d07c, 68 bytes: destroys the instance mesh, if any.
void CMapCell::RemoveInstanceMesh() {
    if (mMeshInstance != 0) {
        LLCCM_DestroyQMesh(mMeshInstance);
        mMeshInstance = 0;
    }
}

// CMapCell::Delete (main app, SimTheme Park 2.04b3; unit v5211), 0x1009d0f4, 1860 bytes. Takes the cell back to its start state (mode 0, no
// neighbours, no flags), by the cell's mode: one jump table of 22 entries (modes above 0x15 do nothing).
//   path   : a counter (mOverlapCounter) counts the deletions; at zero (or `a` and `b` both 0, or the cell system's flag) every neighbour
//            link is cut on both sides and the objects (queues) the neighbours are part of recompute their queue.
//   queue  : the same counter, kept at -1 when the cell is a bend/junction; the value of the object on the cell is paid back to the bank.
//   track  : the track cell of the same place loses its mesh instance.

// Halfwords the original compares ids with, both 0 and never written (names invented).
uint16_t gDeleteNoParent = 0;   // 0x1029e226
uint16_t gDeleteNoThing = 0;    // 0x1029e228

namespace {

const char kThingZero_2[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kNoModify[] = "Removing path cell with no neighbours but NOMODIFY set\n";

// The jump table 0x1029e22c: 0 nothing, 1 the path code, 2 the queue code, 3 the track code.
// (4 is the track code from the point after the track cell's mesh instance is cleared.)
const uint8_t kHandler[22] = {0, 1, 0, 2, 4, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};

CMapCell* CellOf(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// The thing of the entry; entry 0 must be free (a release assert that does nothing here, but the original calls it).
void* ThingOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZero_2);
    return gThingArray[id].mThing;
}

// Pays back the value of the ride (object) on the cell the parent id names, when that cell has a mode.
void PayBack(CMapCell* self) {
    CMapCell* parent = CellOf(self->mParentID);
    if (parent->mType == 0) return;
    const uint16_t id = parent->GetObjectThingId().mIndex;
    CObject* object = static_cast<CObject*>(ThingOf(id));
    const int32_t percentage = object->GetResidualValuePercentage();
    const int32_t cost = LLCC_SystemGetQueueCost();
    const uint32_t product = static_cast<uint32_t>(percentage) * static_cast<uint32_t>(cost);
    const uint32_t amount = static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * product) >> 32) >> 5;
    gGameWorld->GetBankAccount()->MakeDeposit(amount);
}

void ClearCell(CMapCell* self) {
    self->RemoveNeighbour(0xff);
    self->ModifyDirection(0);
    self->mFlags = 0;
    self->mParentID = 0;
    self->ModifyType(static_cast<CELL_MODES>(0));
    self->SetHFieldTile();
}

}  // namespace

void CMapCell::Delete(int32_t a, int32_t b) {
    if (static_cast<int32_t>(mType) == 1 && (mFlags & 0x20) != 0) {
        if (NumberOfNeighbours() == 0) {
            CTRACE(6, kNoModify);
            mFlags = static_cast<uint16_t>(mFlags & 0xffdf);
        }
    }
    if ((mFlags & 0x20) != 0 && gLLCC47E4 == 0) return;
    const uint32_t type = static_cast<uint32_t>(mType);
    if (type > 0x15) return;
    switch (kHandler[type]) {
    case 1: {
        if (a == 0 && b == 0) mOverlapCounter = -1;
        else mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
        if (mOverlapCounter >= 0 && gLLCC47E4 == 0) return;
        mOverlapCounter = 0;
        uint8_t dir = 1;
        do {
            if ((GetNeighbours() & dir) != 0) {
                RemoveNeighbour(dir);
                CMapCell* n = GetNeighbouringCell(dir);
                if (n != nullptr) {
                    n->RemoveNeighbour(OPPOSITE(dir));
                    n->SetHFieldTile();
                    if (static_cast<int32_t>(mType) == 1 && static_cast<int32_t>(n->mType) == 9 &&
                        (dir == 1 || dir == 4 || dir == 0x10 || dir == 0x40)) {
                        const uint16_t parent = n->mParentID;
                        if (parent != gDeleteNoParent) {
                            CMapCell* other = CellOf(parent);
                            const uint16_t id = other->GetObjectThingId().mIndex;
                            if (id != gDeleteNoThing) {
                                CThing* thing = static_cast<CThing*>(ThingOf(id));
                                if (thing->mKind == 3) static_cast<CObject*>(ThingOf(id))->RecalculateBackOfQueue();
                            }
                        }
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 2);
        } while (dir != 1);
        ClearCell(this);
        break;
    }
    case 2: {
        if (a == 0 && b == 0) {
            mOverlapCounter = -1;
        } else {
            uint8_t nb = GetNeighbours();
            if (NumberOfHVNeighbours() == 2) {
                const bool low = (nb & 1) != 0;
                const bool keep = (low && (nb & 4)) || (low && (nb & 0x40)) || ((nb & 0x10) && (nb & 4)) || ((nb & 0x10) && (nb & 0x40));
                if (!keep) nb = 0;
            } else {
                nb = 0;
            }
            if (nb != 0) mOverlapCounter = -1;
            else mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
        }
        if (gLLCC47E4 != 0) {
            mOverlapCounter = 0;
            if (static_cast<int32_t>(mType) != 9) PayBack(this);
            ClearCell(this);
            return;
        }
        if (mOverlapCounter >= 0) return;
        mOverlapCounter = 0;
        if (static_cast<int32_t>(mType) != 9) PayBack(this);
        uint8_t dir = 1;
        do {
            if ((GetNeighbours() & dir) != 0) {
                RemoveNeighbour(dir);
                CMapCell* n = GetNeighbouringCell(dir);
                if (n != nullptr) {
                    n->RemoveNeighbour(OPPOSITE(dir));
                    n->SetHFieldTile();
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
        ClearCell(this);
        break;
    }
    case 3: {
        GetTrackCellEquiv()->mMeshInstance = 0;
        ClearCell(this);
        break;
    }
    case 4:
        ClearCell(this);
        break;
    default:
        break;
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
