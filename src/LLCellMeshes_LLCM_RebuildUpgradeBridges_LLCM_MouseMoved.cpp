// Unit v5633: LLCM_MouseMoved (0x10083180, 6228 bytes), the cell system's mouse move handler: per mode it snaps or clamps the cell
// under the mouse, draws the preview lines, places the ride preview, checks hoarding / clear land / patrol / pylon legality, then sets
// the cursor. Translated from the listing; the words it defines are new here (names invented from their Mac addresses).
#include <cstdint>
#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "MapCell.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ui_main.h"

uint16_t gLLCC9F34 = 0;    // 0x1029df34
int32_t gLLCC9F38 = 1;     // 0x1029df38
int32_t gLLCC9F3C = 1;     // 0x1029df3c
uint16_t gLLCC9F40 = 0;    // 0x1029df40
int32_t gLLCCC20C = 0;     // 0x102dc20c
int32_t gLLCCDB38 = 0;     // 0x102ddb38
int32_t gLLCCDB3C = 0;     // 0x102ddb3c
int32_t gLLCCDB60 = 0;     // 0x102ddb60
int32_t gLLCCDE0B4 = 0;    // 0x102de0b4
int32_t gLLCCDE0B8 = 0;    // 0x102de0b8

namespace {

uint8_t* MMWorldBytes() { return reinterpret_cast<uint8_t*>(gGameWorld); }

// The map cell / track cell of a 1-based cell id (id 0 reaches the slot before the first, as on the Mac).
CMapCell* MMMapCell(uint16_t id) {
    return reinterpret_cast<CMapCell*>(MMWorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

CTrackCell* MMTrackCell(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(MMWorldBytes() + 0x1102d8 + (static_cast<int32_t>(id) - 1) * 0x28);
}

uint16_t MMCellId(int32_t x, int32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1); }

int32_t MMAbs(int32_t v) { return v < 0 ? -v : v; }   // the MSL abs (wraps at INT_MIN under -fwrapv)

int32_t MMSlot(RideInfo* ride, int32_t slot) { return ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

RideInfo* MMRide(uint16_t type) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = type;
    return gRideDatabase.GetRideInfo(id);
}

CELL_MODES MMPreview(int32_t mode) { return static_cast<CELL_MODES>(mode | 0x100); }

// Keeps the move on the axis of the drag start (gLLCC4804, gLLCC4800).
void MMSnap(int32_t& x, int32_t& y) {
    int32_t dy = MMAbs(y - gLLCC4800);
    int32_t dx = MMAbs(x - gLLCC4804);
    if (!(dx < dy)) y = gLLCC4800;
    dy = MMAbs(y - gLLCC4800);
    dx = MMAbs(x - gLLCC4804);
    if (!(dx > dy)) x = gLLCC4804;
}

typedef void (*MMLineFn)(CELL_MODES, int32_t, int32_t, int32_t*, int32_t*);

// The four sides of the rectangle between the drag start and the cell, each line a call (mode < 0: the current mode | 0x100).
void MMRectangle(MMLineFn line, int32_t mode, int32_t step, int32_t minX, int32_t minY, int32_t maxX, int32_t maxY) {
    int32_t endX = maxX;
    int32_t endY = minY;
    int32_t startX = minX;
    if (minX != maxX) {
        startX += step;
        endX -= step;
    }
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), startX, minY, &endX, &endY);
    endX = maxX;
    endY = maxY;
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), maxX, minY, &endX, &endY);
    endX = maxX;
    endY = maxY;
    startX = minX;
    if (minX != maxX) {
        startX += step;
        endX -= step;
    }
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), startX, maxY, &endX, &endY);
    endX = minX;
    endY = minY;
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), minX, maxY, &endX, &endY);
}

// Marks every cell of the potential pylon area where a pylon may stand.
void MMPylonArea() {
    uint32_t x0, y0, x1, y1;
    GetPotentialPylonPositionArea(&x0, &y0, &x1, &y1);
    for (uint32_t j = y0; j < y1; ++j) {
        for (uint32_t i = x0; i < x1; ++i) {
            CMapCell* cell = MMMapCell(MMCellId(static_cast<int32_t>(i), static_cast<int32_t>(j)));
            if (IsValidPylonPosition(i, j, nullptr, 0, 0, 0, 0) == 0) continue;
            if (cell->IsFlag(0x40) != 0) continue;
            if (cell->IsHoarding() != 0) continue;
            if (cell->IsTrack() != 0) continue;
            if (cell->IsEntrance() != 0) continue;
            if (cell->IsExit() != 0) continue;
            if (cell->IsQueue() != 0) continue;
            if (cell->IsRide() != 0) continue;
            if (cell->IsFreeform() != 0) continue;
            if (cell->IsPath() != 0 && cell->IsFlag(0x20) != 0) continue;
            LLCC_DrawLine(static_cast<CELL_MODES>(0x11f), static_cast<int32_t>(i), static_cast<int32_t>(j),
                          reinterpret_cast<int32_t*>(&i), reinterpret_cast<int32_t*>(&j));
        }
    }
}

void MMPlaceRidePreview() {
    gLLCCDB54 = gLLCCDB3C;
    gLLCCDB50 = gLLCCDB38;
    int32_t rotation = LLCC_GetRideRotation();
    if (LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 1) == 0) gLLCC47C8 = 1;
    rotation = LLCC_GetRideRotation();
    LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 0);
    gDynamicCellF9BD4 = 0;
}

// x and y are the caller's: the boundary moves them and the tail reads the moved cell.
void MMRideMode(int32_t& x, int32_t& y) {
    gTrackCostFree = 1;
    RideInfo* ride = MMRide(gLLCCE0E0);
    bool ticketFree = false;
    if (MMSlot(ride, 46) > 0) {   // Mac +0xc4
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = gLLCCE0E0;
        const CGoldenTicketControl* tickets = reinterpret_cast<const CGoldenTicketControl*>(MMWorldBytes() + 0x2c0);
        if (!tickets->HasBeenBought(id)) ticketFree = true;
    }
    if (ticketFree) {
        LLCC_SystemSetSessionCost(0);
    } else {
        LLCC_SystemSetSessionCost(MMSlot(MMRide(gLLCCE0E0), 107));   // Mac +0x1b8
        if (gActionRecorder.mRecording == 0) {
            const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
            if (LLCC_SystemGetSessionCost() > balance) {
                gLLCCC214 = 1;
                gLLCCC204 = 1;
            }
        }
    }
    gLLCC47C8 = 0;
    gLLCCDB3C = x;
    gLLCCDB38 = y;
    if (gLLCCE0E0 != gLLCC9F34) {
        gLLCCDE0B8 = MMSlot(MMRide(gLLCCE0E0), 18);   // Mac +0x54
        if (gLLCCE0DC != 0 && CMap::IsXYValid(x, y) != 0) {
            RideInfo* info = MMRide(gLLCCE0E0);
            int32_t rotation = LLCC_GetRideRotation();
            LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB3C, &gLLCCDB38);
            if (CMap::IsXYValid(x, y) != 0) {
                CTrackCell* track = MMTrackCell(MMCellId(gLLCCDB3C, gLLCCDB38));
                if (track->IsTrackX() != 0) track = MMTrackCell(track->mParentID);
                if (track->IsTrack() != 0) {
                    switch (track->GetDirection()) {
                    case 0x10: rotation = 0xb4; break;
                    case 0x04: rotation = 0x10e; break;
                    case 0x01: rotation = 0; break;
                    case 0x40: rotation = 0x5a; break;
                    default: break;
                    }
                    LLCC_SetRideRotation(rotation, false);
                }
                switch (LLCC_GetRideRotation()) {
                case 0:
                    gLLCCDB3C += MMSlot(info, 38);   // Mac +0xa4
                    gLLCCDB38 += MMSlot(info, 39);   // +0xa8
                    break;
                case 0x10e:
                    gLLCCDB3C += MMSlot(info, 40);   // +0xac
                    gLLCCDB38 += MMSlot(info, 41);   // +0xb0
                    break;
                case 0xb4:
                    gLLCCDB3C += MMSlot(info, 42);   // +0xb4
                    gLLCCDB38 += MMSlot(info, 43);   // +0xb8
                    break;
                case 0x5a:
                    gLLCCDB3C += MMSlot(info, 44);   // +0xbc
                    gLLCCDB38 += MMSlot(info, 45);   // +0xc0
                    break;
                default: break;
                }
                // The current object (gLLCCDB6C, a 32-bit pointer word on the Mac; the host keeps the whole pointer in gLLCMCurrentObject
                // while the two agree), read without a NULL check as the original does.
                const CObject* current =
                    (gLLCCDB6C != 0 && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C))
                        ? gLLCMCurrentObject
                        : nullptr;
                switch (current->mRotation) {
                case 0xb4:
                    gLLCCDB3C -= 1;
                    gLLCCDB38 += 1;
                    break;
                case 0x5a: gLLCCDB3C -= 1; break;
                case 0x10e: gLLCCDB38 += 1; break;
                default: break;
                }
            }
        }
        MMPlaceRidePreview();
    }
    gTrackCostFree = 0;
}

}  // namespace

// 0x10083180 (6228 bytes)
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag) {
    LLCM_SetIllegalPlacement(0);
    gLLCCC214 = 0;
    gLLCCC210 = 0;
    gLLCCC20C = 0;
    gLLCCC208 = 0;
    gLLCCC204 = 0;
    gLLCCC200 = 0;
    gLLCCDB54 = x;
    gLLCCDB50 = y;
    DC_ResetSquareFaceListPtr();
    gLLCCDE0B8 = 0;
    gLLCCDE0B4 = 0;
    if (LLCC_SystemIsMode(0) == 0) {
        if (LLCC_SystemIsMode(1) != 0 || LLCC_SystemIsMode(3) != 0) {
            if (LLCC_SystemIsMode(1) != 0) gLLCCDE0B8 = 2;
            if (LLCC_SystemIsMode(3) != 0) gLLCCDE0B8 = 3;
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                MMSnap(x, y);
                gLLCCDB54 = x;
                gLLCCDB50 = y;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, gLLCC4804, gLLCC4800, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x38) != 0) {
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                const int32_t fromX = gLLCC4804;
                const int32_t fromY = gLLCC4800;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetLastMode());
                LLCC_DrawLine(mode, fromX, fromY, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetLastMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(4) != 0) {
            MMRideMode(x, y);
        } else if (LLCC_SystemIsMode(0x3b) != 0) {
            gTrackCostFree = 1;
            gLLCC47C8 = 0;
            gLLCCDB3C = x;
            gLLCCDB38 = y;
            LLCC_SystemSetSessionCost(MMSlot(MMRide(gLLCCE0E0), 107));
            gLLCCDE0B8 = MMSlot(MMRide(gLLCCE0E0), 18);
            int32_t rotation = LLCC_GetRideRotation();
            if (LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 1) == 0) gLLCC47C8 = 1;
            rotation = LLCC_GetRideRotation();
            LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 0);
            gDynamicCellF9BD4 = 0;
            gTrackCostFree = 0;
        } else if (LLCC_SystemIsMode(0xb) != 0 || LLCC_SystemIsMode(0x10) != 0 || LLCC_SystemIsMode(0x19) != 0) {
            gLLCCDE0B8 = 3;
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                MMSnap(x, y);
                gLLCCDB54 = x;
                gLLCCDB50 = y;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, gLLCC4804, gLLCC4800, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x39) != 0) {
            if (x < gLLCC9F38) x = gLLCC9F38;
            if (y < gLLCC9F3C) y = gLLCC9F3C;
            const int32_t limitX = gLLCCWidth - 2;
            if (limitX < x) x = limitX;
            const int32_t limitY = gLLCCHeight - 3;
            if (limitY < y) y = limitY;
            LLCM_TrackCellBoundary(gHoardingRight, gHoardingBottom, &x, &y, &x, &y);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                gLLCCC214 = LLCC_IsHoardingEditLegal(gLLCC4804, gLLCC4800, x, y, true) == 0 ? 1 : 0;
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                MMRectangle(LLCC_DrawTrackLine, -1, 2, minX, minY, maxX, maxY);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x3a) != 0) {
            LLCC_SystemSetSessionCost(-9999);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                gLLCCC214 = LLCC_IsClearLandLegal(gLLCC4804, gLLCC4800, x, y) == 0 ? 1 : 0;
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                MMRectangle(LLCC_DrawLine, 0x13a, 1, minX, minY, maxX, maxY);
            } else {
                LLCC_DrawLine(static_cast<CELL_MODES>(0x13a), x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x36) != 0) {
            LLCC_SystemSetSessionCost(-9999);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                gLLCCC214 = LLCC_IsPatrolAreaLegal(minX, minY, maxX, maxY) ? 0 : 1;
                MMRectangle(LLCC_DrawLine, -1, 1, minX, minY, maxX, maxY);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0xe) != 0 || LLCC_SystemIsMode(0x13) != 0) {
            if (x != 0 || y != 0) LLCM_StartTrackEditMode(x, y, flag);
        } else if (LLCC_SystemIsMode(0x1a) != 0) {
            if (gLLCCDB68 != gLLCC9F40) {
                LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB54, &gLLCCDB50);
                if (static_cast<int16_t>(gLLCC9F24) == -1) {
                    const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                    LLCC_DrawTrackLine(mode, x, y, &x, &y);
                }
                if (gLLCCReviseAnswer == 0) {
                    LLCC_SystemConstructHoardingClickStack();
                    LLCM_LeftMouseButtonUp(gLLCC4804, gLLCC4800, flag, 0);
                    if (LLCC_SystemIsMode(0x1a) != 0) LLCC_SystemSetModeNR(0x19, true);
                } else if (static_cast<int16_t>(gLLCC9F24) != -1) {
                    gLLCCDB58 = MMCellId(x, y);
                    gLLCC9F26 = static_cast<uint16_t>(MMTrackCell(gLLCCDB58)->GetSegmentNumber());
                    CTrackCell* cell = MMTrackCell(gLLCCDB58);
                    if (cell->ConnectionType(static_cast<CELL_MODES>(LLCC_SystemGetMode()), 0, 0) != 9) {
                        gLLCCDB58 = gLLCCDB5A;
                        gLLCC9F26 = gLLCC9F24;
                    }
                    if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
                        LLCC_TraceTrack(MMTrackCell(gLLCCDB58), MMTrackCell(gLLCCDB5A), 0);
                    else
                        LLCC_TraceTrack(MMTrackCell(gLLCCDB5A), MMTrackCell(gLLCCDB58), 0);
                }
            }
        } else if (LLCC_SystemIsMode(0x15) != 0) {
            gLLCCDE0B8 = 3;
            gLLCCC214 = IsValidPylonPosition(static_cast<uint32_t>(x), static_cast<uint32_t>(y), nullptr, 0, 0, 0, 0) == 0 ? 1 : 0;
            LLCC_DrawLine(static_cast<CELL_MODES>(0x115), x, y, &x, &y);
            MMPylonArea();
        } else if (LLCC_SystemIsMode(0x16) != 0) {
            gLLCCDE0B8 = 3;
            if (gLLCCE090 != 0)
                gLLCCC214 = IsValidPylonPosition(static_cast<uint32_t>(x), static_cast<uint32_t>(y), nullptr, 0, 0, 0, 0) == 0 ? 1 : 0;
            if (gLLCC9F14 == -1) gLLCC9F14 = MMMapCell(MMCellId(x, y))->GetPylonIndex();
            LLCC_DrawLine(static_cast<CELL_MODES>(0x116), x, y, &x, &y);
            if (gLLCCE090 != 0) MMPylonArea();
        }
        // Every mode but 0: the ride under the cell, when there is one, names the info word.
        if (CMap::IsXYValid(x, y) != 0) {
            CMapCell* cell = MMMapCell(MMCellId(x, y));
            if (cell->IsRide() != 0) {
                const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId = MMMapCell(cell->mParentID)->GetObjectThingId();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                const CThing* thing = static_cast<const CThing*>(gThingArray[thingId.mIndex].mThing);
                if (thing->mKind == 3)
                    gLLCCDE0B4 = MMSlot(MMRide(reinterpret_cast<const CObject*>(thing)->mObjectType), 18);
            }
        }
    }
    LLCC_SystemSetCursor();
    gLLCCDB60 = (gLLCC4804 == x && gLLCC4800 == y) ? 1 : 0;
}
