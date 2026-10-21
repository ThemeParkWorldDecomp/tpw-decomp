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
#include "LLCellSystem.h"
#include "ActionRecorder.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "RideDB.h"

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
