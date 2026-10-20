// Unit v5635: LLCR_PlaceRide (0x1008a248, 6,572 bytes; LLCellRide.cpp on the Mac). Places the selected ride on the map at
// (x, y) with its rotation: marks its cells (build != 0), checks them (check != 0) or previews them (both 0), then the
// entrance and exit queue cells and, for a kart / water track ride, the two track blocks.
#include <cstdint>
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "World.h"
#include "TrackCell.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "TPSound.h"
#include "ActionRecorder.h"
#include "ThingReceivers.h"
#include "ppc_math.h"

// The unit's words (BSS on the Mac; names invented).
LLCRPlacement gLLCRPlacement;   // 0x102de0a0
int32_t gLLCRPlaceRotation;     // 0x102de0d0
int32_t gLLCRPlaceY;            // 0x102de0d4
int32_t gLLCRPlaceX;            // 0x102de0d8

namespace {

// The map cell with the 1 based id x + y * 128 + 1 (kept to a halfword, as the Mac does): the grid is at world + 0x2d8.
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

CMapCell* MapCellOf(uint16_t id) {
    CMapCell* base = reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8);
    return base + (static_cast<int32_t>(id) - 1);
}

// The track grid follows the map grid: world + 0x2d8 + 0x110000, 0x28 bytes a cell.
CTrackCell* TrackCellOf(uint16_t id) {
    CTrackCell* base = reinterpret_cast<CTrackCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 + 0x110000);
    return base + (static_cast<int32_t>(id) - 1);
}

CELL_MODES Mode(int32_t m) { return static_cast<CELL_MODES>(m); }

// The current object of the cell system (gLLCCDB6C on the Mac; see gLLCMCurrentObject).
CObject* CurrentObject() {
    if (gLLCMCurrentObject != nullptr &&
        static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject))) == gLLCCDB6C)
        return gLLCMCurrentObject;
    return nullptr;
}

// Joins a queue cell to the path: the cell becomes a path (mode 1) and its path neighbours are recalculated.
void JoinQueueCell(CMapCell* cell) {
    if (static_cast<int32_t>(cell->mType) != 1) cell->Modify(Mode(0x87), 0, 0);
    cell->Modify(Mode(1), 0, 0);
    cell->CalculateNeighbours(Mode(1), 0, 0, true);
    cell->Modify(Mode(0x81), 0, 0);
    cell->Modify(Mode(0x85), 0, 0);
    cell->Modify(Mode(0x86), 0, 0);
    cell->SetFlag(0x20);
    uint8_t dir = 1;
    do {
        if ((cell->GetNeighbours() & dir) != 0) {
            CMapCell* next = cell->GetNeighbouringCell(dir);
            if (next != nullptr && static_cast<int32_t>(next->mType) == 1) {
                next->CalculateNeighbours(Mode(1), 0, 0, true);
                next->Modify(Mode(0x81), 0, 0);
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);
}

}  // namespace

int32_t LLCR_PlaceRide(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t build, int32_t check) {
    return static_cast<int32_t>(reinterpret_cast<uintptr_t>(LLCR_PlaceRideBlock(x, y, a, rotation, build, check)));
}

LLCRPlacement* LLCR_PlaceRideBlock(int32_t x, int32_t y, int32_t /*unused*/, int32_t rotation, int32_t build, int32_t check) {
    gLLCCFirstBlockX = -1;
    gLLCCFirstBlockY = -1;
    gLLCCE0CC = 0;
    gLLCRPlacement.mTopLeftX = -1;
    gLLCRPlacement.mTopLeftY = -1;
    gLLCRPlacement.mEntranceX = -1;
    gLLCRPlacement.mEntranceY = -1;
    gLLCRPlacement.mEntranceDirection = 0;

    uint16_t parentId = 0;
    uint8_t entranceDir = 0;      // 0x94(r1)
    int32_t exitX = 0;            // 0x90(r1)
    int32_t exitY = 0;            // 0x8c(r1)
    uint8_t exitDir = 0;          // 0x8a(r1)
    bool exitSeen = false;        // 0x89(r1)
    bool entranceSeen = false;    // 0x88(r1)
    int32_t entranceX = 0;        // r13
    int32_t entranceY = 0;        // r14
    bool failed = false;          // r16
    int32_t blocks = 0;           // r21: the cells of kind 4
    // r26: the rotation's direction bit, set by the second loop. With no cell in the footprint the Mac passes the caller's r26 on.
    uint8_t dirBase = 0;

    LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE0E0});
    int32_t width, height;
    LLCR_GetWidthHeight(&width, &height);

    const int32_t marginX = (5 - width) / 2;    // 0x9c(r1)
    const int32_t marginY = (5 - height) / 2;   // 0x98(r1)
    const int32_t zeroX = 0;                    // 0x84(r1)
    const int32_t zeroY = 0;                    // 0x80(r1)
    int32_t k7c, k78, k74, k70, k6c, k15;
    if (gLLCC47C4 == 0) {
        x -= width / 2;
        y -= height / 2;
        k7c = marginY - marginX;
        k78 = -1 - marginX - marginY;
        k74 = -1 - marginX - marginX;
        k70 = -1 - marginY - marginY;
        k6c = -1 - marginY - marginX;
        k15 = marginX - marginY;
    } else {
        k15 = 0;
        k7c = 0;
        k78 = -5;
        k74 = -5;
        k70 = -5;
        k6c = -1 - height;
    }
    gLLCRPlaceX = x - marginX;
    gLLCRPlaceY = y - marginY;
    gLLCRPlaceRotation = rotation;

    // The map cell of the footprint's (col, row) under the rotation.
    auto place = [&](int32_t col, int32_t row, int32_t& cx, int32_t& cy) {
        if (rotation == 0xb4) {
            cx = k74 + ((x + 5) - col);
            cy = k70 + ((y + 5) - row);
        } else if (rotation == 0x5a) {
            cx = x + (row + k7c);
            cy = k78 + ((y + 5) - col);
        } else if (rotation == 0x10e) {
            cy = y + (col + k15);
            cx = k6c + ((x + 5) - row);
        } else {
            cx = x + (col + zeroX);
            cy = y + (row + zeroY);
        }
    };

    if (build != 0) {
        for (int32_t row = 0; row < height; ++row) {
            for (int32_t col = 0; col < width; ++col) {
                int32_t cx, cy;
                place(col, row, cx, cy);
                if (!CMap::IsXYValid(cx, cy)) continue;
                CMapCell* cell = MapCellOf(CellId(cx, cy));
                int32_t kind = LLCR_GetRideCell(col, row);
                if (kind == 4) ++blocks;
                if (build != 0 && kind != 0) cell->Modify(Mode(0x87), 0, 0);
            }
        }
        int32_t tx, ty;
        if (rotation == 0xb4) {
            tx = x + k74 + 5;
            ty = y + k70 + 5;
        } else if (rotation == 0x5a) {
            ty = y + k78 + 5;
            tx = x + k7c;
        } else if (rotation == 0x10e) {
            ty = y + k15;
            tx = x + k6c + 5;
        } else {
            tx = x + zeroX;
            ty = y + zeroY;
        }
        gLLCRPlacement.mTopLeftX = tx;
        gLLCRPlacement.mTopLeftY = ty;
        LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{CellId(tx, ty)});
        CObject* ride = LLCR_AddRideToDB(rotation);
        if (gActionRecorder.mRecording == 0) {
            int32_t event = blocks < 4 ? 0x8e : blocks < 8 ? 0x8d : blocks < 0x10 ? 0x8f : 0x90;
            float fx = 10.0f * static_cast<float>(x);
            float fy = 10.0f * static_cast<float>(y);
            SOUND_SendEvent(0, gSoundBanks[3], event, ppc_fctiwz(fx), 0, ppc_fctiwz(fy));
        }
        // The ride's cell bytes (+4 x, +6 y), reached by Mac offset.
        const uint8_t* rb = reinterpret_cast<const uint8_t*>(ride);
        parentId = static_cast<uint16_t>(rb[4] + (static_cast<uint32_t>(rb[6]) << 7) + 1u);
    }

    if (gLLCCE0DC == 0) LLCM_SetCurrentObject(false);

    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            int32_t cx, cy;
            place(col, row, cx, cy);
            dirBase = rotation == 0xb4 ? 0x10 : rotation == 0x5a ? 0x40 : rotation == 0x10e ? 4 : 1;
            if (!CMap::IsXYValid(cx, cy)) continue;
            uint16_t id = CellId(cx, cy);
            CMapCell* cell = MapCellOf(id);
            CTrackCell* track = TrackCellOf(id);
            int32_t kind = LLCR_GetRideCell(col, row);
            uint8_t dir = LLCR_GetRideCellDirection(col, row);
            if (build != 0 && kind != 0) {
                int32_t mode = kind;
                if (mode == 0xb) mode = 4;
                if (mode == 0x10) mode = 4;
                if (mode == 0x17) mode = 4;
                cell->Modify(Mode(0x87), 0, 0);
                cell->Delete(0, 0);
                cell->Modify(Mode(mode), 0, 0);
                cell->Modify(Mode(0x81), 0, 0);
                cell->ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{parentId});
                track->DeleteMesh();
                track->UnSetFlag(0xf);
            }
            switch (static_cast<uint32_t>(kind)) {
            case 0:
                break;
            case 1:   // a path cell of the ride
                if (build != 0) {
                    cell->SetFlag(0x20);
                    cell->CalculateNeighbours(Mode(kind), 0, 0, true);
                    cell->Modify(Mode(0x81), 0, 0);
                    uint8_t d = 1;
                    do {
                        if ((cell->GetNeighbours() & d) != 0) {
                            CMapCell* next = cell->GetNeighbouringCell(d);
                            if (next != nullptr && static_cast<int32_t>(next->mType) == 1) {
                                next->CalculateNeighbours(Mode(1), 0, 0, true);
                                next->Modify(Mode(0x81), 0, 0);
                            }
                        }
                        d = ADD_DIRECTION(d, 4);
                    } while (d != 1);
                } else if (check != 0) {
                    if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            case 10: {   // the exit
                exitSeen = true;
                exitDir = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                switch (exitDir) {
                case 1: exitX = cx; exitY = cy - 1; break;
                case 4: exitY = cy; exitX = cx + 1; break;
                case 0x10: exitX = cx; exitY = cy + 1; break;
                case 0x40: exitY = cy; exitX = cx - 1; break;
                default: break;
                }
                if (build != 0) {
                    cell->ModifyDirection(ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row)));
                    CMapCell* next = cell->GetNeighbouringCell(cell->GetDirection());
                    if (next == nullptr || static_cast<int32_t>(next->mType) != 1) break;
                    next->CalculateNeighbours(Mode(1), 0, 0, true);
                    next->Modify(Mode(0x81), 0, 0);
                } else if (check != 0) {
                    uint8_t d = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    if (cell->Modify(Mode(0x204), 0, d) == 0) return nullptr;
                } else if (row == 0) {
                    if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            }
            case 9: {   // the entrance
                entranceSeen = true;
                entranceDir = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                switch (entranceDir) {
                case 1: entranceX = cx; entranceY = cy + 1; break;
                case 4: entranceY = cy; entranceX = cx - 1; break;
                case 0x10: entranceX = cx; entranceY = cy - 1; break;
                case 0x40: entranceX = cx + 1; entranceY = cy; break;
                default: break;
                }
                if (build != 0) {
                    gLLCRPlacement.mEntranceX = cx;
                    gLLCRPlacement.mEntranceY = cy;
                    gLLCRPlacement.mEntranceDirection = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    cell->ModifyDirection(gLLCRPlacement.mEntranceDirection);
                    CMapCell* next = cell->GetNeighbouringCell(OPPOSITE(cell->GetDirection()));
                    if (next == nullptr || static_cast<int32_t>(next->mType) != 1) break;
                    next->CalculateNeighbours(Mode(1), 0, 0, true);
                    next->Modify(Mode(0x81), 0, 0);
                } else if (check != 0) {
                    uint8_t d = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    if (cell->Modify(Mode(0x204), 3, d) == 0) return nullptr;
                } else if (row == 0) {
                    if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            }
            case 11:   // a kart track block
            case 16: { // a water track block
                const int32_t checked = kind == 11 ? 0x20b : 0x210;
                const int32_t plain = kind == 11 ? 0x10b : 0x110;
                if (build == 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(checked), 0, 0) == 0) return nullptr;
                        if (cell->IsRide() != 0) return nullptr;
                    } else if (cell->Modify(Mode(plain), 0, 0) == 0) {
                        failed = true;
                    }
                } else {
                    cell->SetFlag(0x10);
                    track->SetFlag(0x10);
                }
                break;
            }
            case 23: {   // the cell beside the ride the direction points at
                if (dir != 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                    } else if (build == 0) {
                        cell->Modify(Mode(0x104), 0, ADD_DIRECTION(dirBase, 0x10));
                    }
                }
                CMapCell* next = cell->GetNeighbouringCell(ADD_DIRECTION(dirBase, dir));
                if (check != 0) {
                    if (next == nullptr) return nullptr;
                    if (next->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                } else if (build == 0) {
                    if (next != nullptr)
                        next->Modify(Mode(0x104), 5, ADD_DIRECTION(dirBase, 0x10));
                    else
                        gLLCCC214 = 1;
                } else {
                    // No NULL check here (the original's).
                    next->Modify(Mode(0x87), 0, 0);
                    next->Modify(Mode(0x15), 0, 0);
                    next->Modify(Mode(0x83), 0, 0);
                    next->Modify(Mode(0x84), 0, 0);
                    next->Modify(Mode(0x81), 0, 0);
                }
                break;
            }
            default:   // 2 to 8, 12 to 15, 17 to 22, and every kind above 23 (unsigned)
                if (build == 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                    } else if (row == 0) {
                        if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                    } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                        failed = true;
                    }
                }
                break;
            }
        }
    }

    if (gLLCCE0DC == 0) LLCM_SetCurrentObject(false);
    entranceDir = OPPOSITE(entranceDir);
    const uint8_t exitBack = OPPOSITE(exitDir);

    if (entranceSeen || exitSeen) {
        if (failed) gLLCCC214 = 1;
        if (!CMap::IsXYValid(entranceX, entranceY)) return nullptr;
        CMapCell* queue = MapCellOf(CellId(entranceX, entranceY));
        if (LLCC_SystemGetRideQueue() != 0) {
            if (check != 0) {
                if (queue->Modify(Mode(0x204), 3, entranceDir) == 0) return nullptr;
            } else if (build != 0) {
                CMapCell* door = MapCellOf(CellId(gLLCRPlacement.mEntranceX, gLLCRPlacement.mEntranceY));
                door->ModifyNeighbours(entranceDir);
                door->ModifyDirection(entranceDir);
                queue->Modify(Mode(0x87), 0, 0);
                queue->Modify(Mode(3), 0, 0);
                queue->ModifyNeighbours(OPPOSITE(entranceDir));
                queue->ModifyDirection(OPPOSITE(entranceDir));
                queue->ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{door->mParentID});
                queue->SetFlag(0x20);
                queue->Modify(Mode(0x81), 0, 0);
                queue->Modify(Mode(0x85), 0, 0);
                queue->Modify(Mode(0x86), 0, 0);
                CurrentObject()->RecalculateBackOfQueue();
            } else {
                queue->Modify(Mode(0x104), 3, entranceDir);
            }
        } else if (check != 0) {
            if (queue->Modify(Mode(0x201), 7, 0) == 0) return nullptr;
        } else if (build != 0) {
            JoinQueueCell(queue);
        } else {
            queue->Modify(Mode(0x118), 7, entranceDir);
        }

        if (LLCC_SystemGetRideQueue() != 0) {
            if (!CMap::IsXYValid(exitX, exitY)) return nullptr;
            CMapCell* out = MapCellOf(CellId(exitX, exitY));
            if (check != 0) {
                if (out->Modify(Mode(0x201), 0, exitBack) == 0) return nullptr;
            } else if (build != 0) {
                JoinQueueCell(out);
            } else {
                out->Modify(Mode(0x118), 4, exitBack);
            }
        }
    }

    int32_t trackKind = LLCC_SystemGetRideTrack();
    if (trackKind >= 3 || trackKind < 1) return &gLLCRPlacement;

    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE0E0});
    auto adjust = [&](int32_t slot) { return info->Value(static_cast<RideBalanceSlot>(slot)).mInteger; };
    int32_t startX, startY, endX, endY;
    if (rotation == 0xb4) {
        int32_t bx = adjust(kRideSlot_SouthXAdjust) + (((x + 4) - marginX) - marginX);
        endX = bx - 4;
        startX = bx - 9;
        int32_t by = ((y + 4) - marginY) - marginY;
        endY = adjust(kRideSlot_SouthYAdjust) + (by - 1);
        startY = adjust(kRideSlot_SouthYAdjust) + (by - 3);
    } else if (rotation == 0x5a) {
        int32_t bx = adjust(kRideSlot_WestXAdjust) + ((x + marginY) - marginX);
        endX = bx - 1;
        startX = bx;
        int32_t by = ((y + 4) - marginX) - marginY;
        endY = adjust(kRideSlot_WestYAdjust) + (by + 2);
        startY = adjust(kRideSlot_WestYAdjust) + (by - 3);
    } else if (rotation == 0x10e) {
        int32_t bx = adjust(kRideSlot_EastXAdjust) + (((x + 4) - marginY) - marginX);
        endX = bx - 1;
        startX = bx - 3;
        int32_t by = (y + marginX) - marginY;
        startY = adjust(kRideSlot_EastYAdjust) + by;
        endY = adjust(kRideSlot_EastYAdjust) + (by - 4);
    } else {
        int32_t bx = adjust(kRideSlot_NorthXAdjust) + x;
        endX = bx + 8;
        startX = bx;
        startY = adjust(kRideSlot_NorthYAdjust) + y;
        endY = adjust(kRideSlot_NorthYAdjust) + (y - 1);
    }
    if (static_cast<uint8_t>(LLCC_DrawKartBlock(startX, startY, rotation, build, check, dirBase)) == 0) return nullptr;
    LLCR_OffsetEndOfTrack(&endX, &endY, rotation);
    if (static_cast<uint8_t>(LLCC_DrawKartBlock(endX, endY, rotation, build, check, dirBase)) == 0) return nullptr;
    return &gLLCRPlacement;
}
