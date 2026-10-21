// Unit v5932: LLCR_OverRide (the pointer overload, 0x1008905c), LLCM_TrackCellBoundary, LLCR_SetRide, LLCR_AddRideToDB.
#include <cstdint>
#include <new>
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "UIInterface.h"
#include "ThingReceivers.h"
#include "BMP_InsideArea.h"
#include "ObjectParts.h"
#include "analyser.h"
#include "Debug.h"
#include "ppc_math.h"
#include "platform/arena.h"

// The words of this unit (initialised data of the Mac, zero at start).
uint16_t gLLCCE04C = 0;
uint16_t gLLCCE092 = 0;
int32_t gLLCCE094 = 0;
const uint8_t* gLLCRRideShape = nullptr;
RideInfo* gLLCCE098 = nullptr;
uint16_t gLLCCE09C = 0;

namespace {
// A balance value of a ride record (the Mac reads the word at 0x00c + 4 * slot; the host record keeps 8-byte values behind a
// 64-bit table header, so the Mac's offsets name other bytes here). Unit v5L28.
inline int32_t RideValue(RideInfo* ride, RideBalanceSlot slot) { return ride->Value(slot).mInteger; }
}  // namespace

// 0x1008905c: x and y are updated in place; the answer is the thing id of the cell under (x, y), 0 when there is none.
void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y) {
    if (CMap::IsXYValid(*x, *y) == 0) {
        *out = 0;
        return;
    }
    uint16_t id = static_cast<uint16_t>(*x + static_cast<int32_t>(static_cast<uint32_t>(*y) << 7) + 1);
    char* mapBase = reinterpret_cast<char*>(gGameWorld) + 0x2d8;
    char* cell = mapBase + static_cast<int32_t>(id - 1) * 0x44;
    int32_t kind = *reinterpret_cast<int32_t*>(cell + 8);
    if (kind != 4 && kind != 9 && kind != 10) {
        *out = 0;
        return;
    }
    if (*reinterpret_cast<uint16_t*>(cell + 0x10) != id) {
        id = *reinterpret_cast<uint16_t*>(cell + 0x10);
        cell = mapBase + static_cast<int32_t>(id - 1) * 0x44;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CMapCell*>(cell)->GetObjectThingId();
    uint16_t thingId = thing.mIndex;
    if (thingId != gLLCCE04C) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr,
                       "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        id = *reinterpret_cast<uint16_t*>(static_cast<char*>(gThingArray[thingId].mThing) + 0x30);
    }
    *x = static_cast<int32_t>(id - 1) & 0x7f;
    *y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(id - 1)) >> 7);
    *out = thingId;
}

// 0x100889ac: snaps (*cellX, *cellY) down to the even offset from (x, y); both outputs get the same pair.
void LLCM_TrackCellBoundary(int32_t x, int32_t y, int32_t* cellX, int32_t* cellY, int32_t* outX, int32_t* outY) {
    int32_t dx = *cellX - x;
    int32_t dy = *cellY - y;
    if (dx < 0 && dx - (dx / 2) * 2 != 0) dx -= 1;
    if (dy < 0 && dy - (dy / 2) * 2 != 0) dy -= 1;
    int32_t nx = x + (dx / 2) * 2;
    int32_t ny = y + (dy / 2) * 2;
    *cellX = nx;
    *outX = nx;
    *cellY = ny;
    *outY = ny;
}

// 0x1008bc20
void LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    gLLCCE09C = id.mIndex;
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> again{gLLCCE09C};
    gLLCCE098 = gRideDatabase.GetRideInfo(again);
    // The word 0x18 is the shape slot: a native pointer here, to a block the balance loader takes from the heap, so the host keeps
    // it whole beside the word. (Unit v5L28: the word at the Mac's offset 0x18 of the host record, read as an arena offset, sent
    // LLCR_GetWidthHeight and LLCR_GetRideCell to random addresses while a bought ride was placed.)
    gLLCRRideShape = static_cast<const uint8_t*>(gLLCCE098->Value(kRideSlot_Shape).mShape);
    gLLCCE094 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gLLCRRideShape));
}

// 0x1008be1c. A rotation other than 0, 0x5a, 0xb4 or 0x10e leaves the track kind (r28) unset on the Mac: it is taken as 0 here.
CObject* LLCR_AddRideToDB(int32_t rotation) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideId{gLLCCE09C};
    RideInfo* ride = gRideDatabase.GetRideInfo(rideId);
    int32_t built = 0;
    if (RideValue(ride, kRideSlot_BumperType) != 0) {
        uint16_t cellId = gLLCCE092;
        int32_t x = static_cast<int32_t>(cellId - 1) & 0x7f;
        int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(cellId - 1)) >> 7);
        int32_t kind = 0;
        if (rotation == 0xb4) {
            kind = 6;
            x += RideValue(ride, kRideSlot_SouthXAdjust);
            y += RideValue(ride, kRideSlot_SouthYAdjust);
        } else if (rotation == 0x5a) {
            kind = 5;
            x += RideValue(ride, kRideSlot_EastXAdjust);
            y += RideValue(ride, kRideSlot_EastYAdjust);
        } else if (rotation == 0) {
            kind = 0;
            x += RideValue(ride, kRideSlot_NorthXAdjust);
            y += RideValue(ride, kRideSlot_NorthYAdjust);
        } else if (rotation == 0x10e) {
            kind = 1;
            x += RideValue(ride, kRideSlot_WestXAdjust);
            y += RideValue(ride, kRideSlot_WestYAdjust);
        }
        LLCR_OffsetStartOfTrack(&x, &y, rotation);
        float fy = static_cast<float>(x);   // x goes to the second argument
        float fx = static_cast<float>(y);
        fy = 10.0f * fy;
        fx = 10.0f * fx;
        fy = 307.2f * fy;
        fx = 307.2f * fx;
        int32_t a = ppc_fctiwz(fy);
        int32_t b = ppc_fctiwz(fx);
        built = BMP_CreateRide(RideValue(ride, kRideSlot_BumperType), a, b, kind, 0, gLLCCE09C);
    }
    MapDelta delta = reinterpret_cast<MapDelta*>(&ride->mMapOffsetX)->Rotate(rotation);   // the record's 0x7ac
    uint16_t at = static_cast<uint16_t>(delta.mX + static_cast<int32_t>(static_cast<uint32_t>(delta.mY) << 7) + gLLCCE092);
    void* block = tpw::ArenaAlloc(sizeof(CObject));   // the Mac's game heap: the arena (unit v5L31; a list keeps a thing's address in 32 bits)
    CObject* object = block == nullptr ? nullptr
                                       : new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{at},
                                                             tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE09C}, rotation, built);
    if (object != nullptr) {
        if (gLLCCE0DC != 0 && RideValue(ride, kRideSlot_UpgradeType) != 2) {
            CObject* current = LLCM_GetCurrentObject();
            object->SetUpgradeParent(reinterpret_cast<CThing*>(current)->GetThingId());
        } else {
            object->SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0});
        }
        gUIInterface.SetSelectedObject(object);
    }
    return object;
}
