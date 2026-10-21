// LLCellMouse.cpp of the main app (unit v5182): LLCM_SetCurrentObject 0x10088590 (832 bytes) and LLCM_StartTrackEditMode 0x10088a64 (1,372 bytes).
#include <cstdint>

#include "ActionRecorder.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"

// New words of the unit (initialised data of the image, the real names are unknown).
int32_t gLLCC9F18 = -1;         // 0x1029df18
int32_t gLLCC9F1C = -1;         // 0x1029df1c
int32_t gLLCC9F20 = -1;         // 0x1029df20
uint16_t gLLCCE028 = 0;         // 0x1029e028
uint16_t gLLCCE02A = 0;         // 0x1029e02a  (the image holds 0; unit v5L39, was 0xffff)
int32_t gLLCCE034 = -1;         // 0x1029e034
int32_t gLLCCE038 = -1;         // 0x1029e038
CTrackCell* gLLCCDB48 = nullptr;   // 0x102ddb48
uint16_t gLLCCDB5C = 0;         // 0x102ddb5c
CObject* gLLCMCurrentObject = nullptr;   // host only, see LLCellSystem.h

namespace {

// The track cell of a 1 based cell id: the world + 0x1102d8 + (id - 1) * 0x28 (plain arithmetic, id 0 gives the cell before the first).
CTrackCell* CellOf(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x1102d8 +
                                         static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// (x + (y << 7) + 1) & 0xffff
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// The current object: gLLCCDB6C is a 32-bit word (a pointer on the Mac), so the whole pointer is kept in gLLCMCurrentObject beside it.
CObject* Current() {
    uint32_t word = static_cast<uint32_t>(gLLCCDB6C);
    if (word != 0 && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == word) return gLLCMCurrentObject;
    return nullptr;
}

void SetCurrent(CObject* object) {
    gLLCMCurrentObject = object;
    gLLCCDB6C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(object));
}

// A balance value of a ride record (the Mac reads the word at 0x00c + 4 * slot; the host record keeps 8-byte values behind a
// 64-bit table header, so the Mac's offsets name other bytes here). Unit v5L28.
int32_t RideValue(const RideInfo* info, RideBalanceSlot slot) {
    return info->Value(slot).mInteger;
}

// The cell id of an object's cell (its x and y bytes).
uint16_t ObjectCellId(const CObject* object) { return CellId(object->mBase.mCellX, object->mBase.mCellY); }

}  // namespace

void LLCM_SetCurrentObject(bool flag) {
    gLLCCDB5C = gLLCCDB68;
    gLLCCDB68 = 0;
    gLLCC9F18 = -1;
    gLLCC9F1C = -1;
    gLLCC9F20 = -1;
    bool done = false;
    if (LLCC_SystemIsMode(0x19) != 0 || LLCC_SystemIsMode(0x1a) != 0) {
        int32_t right = gHoardingRight;
        if (right != -1) {
            int32_t bottom = gHoardingBottom;
            gLLCCDB68 = CellId(right, bottom);
            gLLCCDB48 = CellOf(gLLCCDB68);
            gLLCC9F18 = right;
            gLLCCFirstBlockX = right;
            gLLCC9F1C = bottom;
            gLLCCFirstBlockY = bottom;
            gLLCC9F20 = 0;
            gLLCCE034 = gLLCCFirstBlockX;
            gLLCCE038 = gLLCCFirstBlockY;
            gLLCCE0CC = 4;
            LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
            done = true;
        }
    }
    if (!done) {
        CObject* object = gUIInterface.GetSelectedObject();
        SetCurrent(object);
        if (object != nullptr) {
            if (object->GetUpgradeParent().mIndex != gLLCCE028) {
                uint16_t parent = object->GetUpgradeParent().mIndex;
                // (the Mac reads the object again from the word)
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                CObject* upgrade = static_cast<CObject*>(gThingArray[parent].mThing);
                RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Current()->mObjectType});
                if (RideValue(info, kRideSlot_UpgradeType) == 1) {
                    SetCurrent(upgrade);
                    gUIInterface.SetSelectedObject(upgrade);
                }
            }
            CObject* cur = Current();
            gLLCCDB68 = ObjectCellId(cur);
            gLLCCDB48 = CellOf(gLLCCDB68);
            uint32_t index = static_cast<uint32_t>(gLLCCDB68) - 1u;
            gLLCC9F18 = static_cast<int32_t>(index & 0x7f);
            gLLCCFirstBlockX = gLLCC9F18;
            gLLCC9F1C = static_cast<int32_t>(index >> 7);
            gLLCCFirstBlockY = gLLCC9F1C;
            gLLCC9F20 = cur->mRotation;
            gLLCCE034 = gLLCCFirstBlockX;
            gLLCCE038 = gLLCCFirstBlockY;
            gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
            LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
        }
    }
    CObject* cur = Current();
    if (cur != nullptr) {
        if (gLLCCDB68 != gLLCCDB5C && flag) gActionRecorder.RecordAction(0x32, ObjectCellId(cur));
    }
}

void LLCM_StartTrackEditMode(int32_t x, int32_t y, uint16_t) {
    if (Current() != nullptr) {
        RideInfo* info = Current()->GetDBInfo();
        if (info != nullptr) {
            LLCC_SystemSetRideQueue(RideValue(info, kRideSlot_HasQueue) != 0 ? 1 : 0);
            LLCC_SystemSetRideTrack(RideValue(info, kRideSlot_WhichTrackType));
        }
    }
    int32_t track = LLCC_SystemGetRideTrack();
    if (track == 2) {
        if (LLCC_SystemIsMode(0x13) == 0) LLCC_SystemSetModeNR(0x13, true);
    } else if (track < 2) {
        if (track != 0 && track >= 0) {
            if (LLCC_SystemIsMode(0xe) == 0) LLCC_SystemSetModeNR(0xe, true);
        }
    } else if (track == 4) {
        if (LLCC_SystemIsMode(0x1a) == 0) LLCC_SystemSetModeNR(0x1a, true);
    }
    if (gLLCCDB68 == gLLCCE02A) return;

    int32_t startX = gLLCC9F18;
    int32_t startY = gLLCC9F1C;
    gLLCCFirstBlockX = startX;
    gLLCCFirstBlockY = startY;
    gLLCC9F20 = Current()->mRotation;
    gLLCCE034 = startX;
    gLLCCE038 = startY;
    gLLCCE0CC = LLCR_OffsetStartOfTrack(&startX, &startY, gLLCC9F20);
    LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
    int32_t dx = x - startX;
    int32_t dy = y - startY;
    if (dx < 0 && dx != (dx / 2) * 2) dx -= 1;
    if (dy < 0 && dy != (dy / 2) * 2) dy -= 1;
    x = startX + (dx / 2) * 2;
    gLLCCDB54 = x;
    y = startY + (dy / 2) * 2;
    gLLCCDB50 = y;
    if (static_cast<int16_t>(gLLCC9F24) == -1) {
        int32_t endX = x;
        int32_t endY = y;
        LLCM_CoverUpgrade(&endX, &endY);
        int32_t mode = LLCC_SystemGetMode();
        LLCC_DrawTrackLine(static_cast<CELL_MODES>(mode | 0x100), x, y, &endX, &endY);
    }
    CObject* object = Current();
    if (object->mF28 == 0) {
        LLCC_SystemConstructTrackClickStack(object);
        gDynamicCellF9BC4 = static_cast<float>(gLLCC4804);
        gDynamicCellF9BC0 = static_cast<float>(gLLCC4800);
    } else if (static_cast<int16_t>(gLLCC9F24) != -1) {
        CTrackCell* first = CellOf(CellId(x, y));
        int32_t coverX = x;
        int32_t coverY = y;
        LLCM_CoverUpgrade(&coverX, &coverY);
        CTrackCell* covered = CellOf(CellId(coverX, coverY));
        int16_t firstSegment = first->GetSegmentNumber();
        int16_t coveredSegment = covered->GetSegmentNumber();
        if (coveredSegment > firstSegment) {
            x = coverX;
            y = coverY;
        }
        gLLCCDB58 = CellId(x, y);
        gLLCC9F26 = static_cast<uint16_t>(CellOf(gLLCCDB58)->GetSegmentNumber());
        CTrackCell* cell = CellOf(gLLCCDB58);
        int32_t mode = LLCC_SystemGetMode();
        if (cell->ConnectionType(static_cast<CELL_MODES>(mode), 0, 0) != 9) {
            gLLCCDB58 = gLLCCDB5A;
            gLLCC9F26 = gLLCC9F24;
            gLLCMIllegalPlacement = 0;
            gLLCCC214 = 0;
            gLLCCC210 = 0;
        }
        if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26)) {
            LLCC_TraceTrack(CellOf(gLLCCDB58), CellOf(gLLCCDB5A), 0);
        } else {
            LLCC_TraceTrack(CellOf(gLLCCDB5A), CellOf(gLLCCDB58), 0);
        }
    }
}
