// CMapCell::ModifyType 0x10099614 (740 bytes) and CMapCell::GetTile 0x1009bc54 (1108 bytes), main app (unit v5535).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
#include "LLCellSystem.h"
#include "ActionRecorder.h"
#include "ThingReceivers.h"

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
