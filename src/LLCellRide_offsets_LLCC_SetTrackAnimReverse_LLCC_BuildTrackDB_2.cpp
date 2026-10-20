// Unit v5982: LLCC_BuildTrackDB 0x1008ce04 (4,048 bytes). Walks the track from the first block (gLLCCFirstBlockX / Y, moved four cells
// by the rotation gLLCC9F20) cell by cell, two cells per step, in the direction byte gLLCCE0CC. For every track cell it places the
// ride's track sections (BMP_TrackSectionPlaceSection, only when `flag` is false), numbers the segments, turns the corners and, for
// the cells that carry the start flag 0x10, switches the "disabled" bit of the ride animation's nodes. Answers nonzero while the
// track is whole (the walk got back to its start without a break).
#include <cstdint>
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "CellBase.h"
#include "TrackCell.h"
#include "World.h"
#include "TPEngine.h"
#include "rsse.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "m3d2.h"
#include "Debug.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "ThingReceivers.h"

// Two words of the image that no other file names (initialised data).
uint16_t gLLCCE0B0 = 0;        // 0x1029e0b0
uint16_t gLLCCE0B2 = 0xffff;   // 0x1029e0b2

namespace {

const char kThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kFoundUpgrade[] = "Found upgrade... modify?\n";

// The track cell of a packed cell id (1 + x + (y << 7)): the id 0 lands one cell before the grid, as on the Mac.
CTrackCell* CellOf(uint32_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int32_t>(id - 1) * 0x28 + 0x110000);
}

// The scale the original applies to a cell coordinate: single precision, 10 times 307.2, truncated.
int32_t Scaled(int32_t cell) {
    float value = static_cast<float>(cell);
    float tenth = 10.0f * value;
    float scaled = 307.2f * tenth;
    return static_cast<int32_t>(scaled);
}

void Place(int32_t ride, int32_t x, int32_t y, int32_t kind, int32_t count, uint32_t thing, int32_t info) {
    BMP_TrackSectionPlaceSection(ride, Scaled(x), Scaled(y), kind, count, thing, info);
}

// The node (or mini node) of a ride animation set that a data node index names; its first word is the flag word.
uint32_t* NodeFlags(tag_sRideAnimSet* set, int32_t index) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t i = static_cast<uint32_t>(index) + header->mF46;
    if (i < header->mNodeCount) return reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(header->mNodes.get()) + i * 0xa0);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(header->mMiniNodes.get()) + (i - header->mNodeCount) * 0x58);
}

void SetNode(tag_sRideAnimSet* set, int32_t index) { *NodeFlags(set, index) |= 0x80000010u; }
void ClearNode(tag_sRideAnimSet* set, int32_t index) { *NodeFlags(set, index) &= 0xffffffefu; }

// Whether a neighbouring cell makes the flagged cell's end "closed": a track cell with a mesh, or a cell of type 0xd / 0x12.
bool Closed(CTrackCell* cell) {
    if (cell == nullptr) return false;
    if (cell->IsJustTrack() != 0 && cell->mMeshInstance != 0) return true;
    return cell->mType == 0xd || cell->mType == 0x12;
}

}  // namespace

int32_t LLCC_BuildTrackDB(bool flag) {
    int32_t segment = 0;
    int32_t count = 0;        // r23: the flagged cells met
    uint32_t thing = 0;       // r20
    int32_t info = 0;         // r13
    int32_t whole = 1;        // r14
    uint32_t mask = 0;        // r25
    int32_t rotation = gLLCC9F20;
    int32_t startY = gLLCCFirstBlockY;
    int32_t startX = gLLCCFirstBlockX;
    uint8_t direction = gLLCCE0CC;
    if (rotation == 0xb4) {
        startX = startX + 4;
    } else if (rotation < 0xb4) {
        if (rotation == 0x5a) {
            startY = startY + 4;
        } else if (rotation < 0x5a && rotation == 0) {
            startX = startX - 4;
        }
    } else if (rotation == 0x10e) {
        startY = startY - 4;
    }
    int32_t x = startX;
    int32_t y = startY;
    CTrackCell* cell = CellOf(static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1));
    if (cell->IsTrackX() != 0) cell = CellOf(cell->mParentID);
    if (cell->IsTrack() == 0) return 0;

    CCellBase* parent = CellOf(cell->mParentID);
    CMapCell* mapCell = parent->GetMapCellEquiv();
    uint16_t thingId = mapCell->GetObjectThingId().mIndex;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    int32_t ride = static_cast<int32_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(gThingArray[thingId].mThing) + 0x24));
    if (!flag) BMP_FreeTrack(ride);

    do {
        cell = CellOf(static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1));
        cell->GetMapCellEquiv();
        if (cell->IsTrack() == 0) {
            direction = 0;
        } else {
            if (cell->IsCorner() != 0) {
                if (!flag) {
                    uint8_t corner = cell->IsCorner();
                    if (corner == 0x41) {
                        Place(ride, x, y, 6, 0, 0, 0);
                    } else if (corner < 0x41) {
                        if (corner == 0x14) {
                            Place(ride, x, y, 8, 0, 0, 0);
                        } else if (corner < 0x14 && corner == 5) {
                            Place(ride, x, y, 5, 0, 0, 0);
                        }
                    } else if (corner == 0x50) {
                        Place(ride, x, y, 7, 0, 0, 0);
                    }
                    count = 0;
                    thing = 0;
                }
            } else if (cell->IsCrossRoad() != 0) {
                if (!flag) {
                    Place(ride, x, y, 0xb, 0, 0, 0);
                    count = 0;
                    thing = 0;
                }
            } else if (cell->IsStraight() != 0) {
                int32_t high = segment < 2 ? 0x10000 : 0;
                if (cell->IsFlag(0x10) != 0) {
                    int32_t px = x;
                    int32_t py = y;
                    uint16_t found;
                    if (x < gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1 || gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1 < x ||
                        y != gMainBalance.mValues[(0x580 - 8) / 4].mInteger) {
                        LLCR_OverRide(&found, &px, &py);
                    } else {
                        found = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
                    }
                    if (found != gLLCCE0B0) {
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
                        CObject* object = static_cast<CObject*>(gThingArray[found].mThing);
                        if (object != nullptr) {
                            thing = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(object) + 0x1c);
                            info = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(object->GetDBInfo()) + 0x10);
                        }
                    }
                } else {
                    count = 0;
                    thing = 0;
                }
                if (!flag) {
                    uint8_t straight = cell->IsStraight();
                    if (straight == 0x44) {
                        int32_t kind = high | (cell->IsFlag(0x10) != 0 ? 0xc : 9);
                        Place(ride, x, y, kind, count, thing, info);
                    } else if (straight < 0x44 && straight == 0x11) {
                        int32_t kind = high | (cell->IsFlag(0x10) != 0 ? 0xd : 0xa);
                        Place(ride, x, y, kind, count, thing, info);
                    }
                }
            }

            if (cell->IsFlag(0x10) != 0) {
                uint32_t id = cell->mCellId;
                count = count + 1;
                int32_t cx = static_cast<int32_t>((id - 1) & 0x7f);
                int32_t cy = static_cast<int32_t>((id - 1) >> 7);
                if (count == 0) mask = 0;
                CTRACE(6, kFoundUpgrade);
                uint16_t found;
                if (cx < gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1 || gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1 < cx ||
                    cy != gMainBalance.mValues[(0x580 - 8) / 4].mInteger) {
                    LLCR_OverRide(&found, &cx, &cy);
                } else {
                    found = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
                }
                if (found != gLLCCE0B2) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
                    if (gThingArray[found].mThing != nullptr) {
                        uint32_t ride2 = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(gThingArray[found].mThing) + 0x1c);
                        CTrackCell* next = cell->GetNeighbouringCell(cell->GetDirection(), 2);
                        if (Closed(next)) mask |= 0x10;
                        next = cell->GetNeighbouringCell(OPPOSITE(cell->GetDirection()), 2);
                        if (Closed(next)) mask |= 1;
                        if (count == 2) {
                            if (mask == 0x10) {
                                int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                            } else if (mask < 0x10) {
                                if (mask == 1) {
                                    int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                    int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                    if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                    if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                                } else if (mask == 0) {
                                    int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                    if (a != -1) ClearNode(gRideAnimSets[ride2], a);
                                    int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                    if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                                }
                            } else if (mask < 0x12) {
                                int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                if (b != -1) SetNode(gRideAnimSets[ride2], b);
                            }
                        }
                    }
                }
            } else if (cell->mMeshInstance == 0 && cell->IsJustTrack() != 0) {
                cell->SetHFieldTile();
            }

            if (cell->IsCorner() != 0) {
                uint8_t opposite = OPPOSITE(direction);
                if ((opposite & cell->GetNeighbours()) == 0) opposite = direction;
                direction = static_cast<uint8_t>(opposite ^ cell->GetNeighbours());
                if (direction != cell->GetDirection()) {
                    cell->ModifyDirection(direction);
                    cell->SetHFieldTile();
                }
            }
        }

        if (direction == 0 || cell->NumberOfNeighbours() < 2) whole = 0;
        if (cell->NumberOfNeighbours() == 3) {
            CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
            if (next->IsTrack() != 0) {
                CTrackCell* again = cell->GetNeighbouringCell(direction, 2);
                uint8_t neighbours = again->GetNeighbours();
                if ((direction & (neighbours == 0 ? 1 : 0)) != 0) whole = 0;
            }
            if (cell->GetNeighbouringCell(direction, 2)->mType == 0) whole = 0;
        }
        cell->SetSegmentNumber(static_cast<int16_t>(segment));
        segment = segment + 1;
        if (direction == 0x10) {
            y = y + 2;
        } else if (direction < 0x10) {
            if (direction == 4) {
                x = x + 2;
            } else if (direction < 4 && direction == 1) {
                y = y - 2;
            }
        } else if (direction == 0x40) {
            x = x - 2;
        }
    } while ((x != startX || y != startY) && whole != 0);
    return whole;
}
