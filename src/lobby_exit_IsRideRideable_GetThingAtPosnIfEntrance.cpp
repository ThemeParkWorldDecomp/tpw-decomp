// Unit v51013: GetThingAtPosnIfEntrance(int, int), MoveCameraOverRideExit (camera over a ride's exit) and the collision object
// helpers of the advisor world (FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide).
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "MapCell.h"
#include "Ptr32.h"
#include "TPCamera.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ppc_math.h"

namespace {

template <class T>
T& At(void* base, int32_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

float FloatFromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

}  // namespace

// 0x10032618 (308 bytes): the grid square's cell, when it is an entrance, names (halfword +0x10) the cell of the ride whose list of things
// (first id at +0x24, next ids at thing +0x0a, ended by gThingListEnd) is walked for the first thing of kind 3 whose ride record has 0 at +0x118.
CObject* GetThingAtPosnIfEntrance(int32_t gridX, int32_t gridZ) {
    const uint16_t index = static_cast<uint16_t>(gridX + (gridZ << 7) + 1);
    char* const map = reinterpret_cast<char*>(gGameWorldRef) + 0x2d8;
    CMapCell* const cell = reinterpret_cast<CMapCell*>(map + (static_cast<int32_t>(index) - 1) * 0x44);
    if (cell->IsEntrance() != 0) {
        const uint16_t link = At<uint16_t>(cell, 0x10);
        char* const target = map + (static_cast<int32_t>(link) - 1) * 0x44;
        if (target != nullptr) {
            uint16_t id = At<uint16_t>(target, 0x24);
            while (id != gThingListEnd) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZeroMessage);
                char* const thing = static_cast<char*>(gThingArray[id].mThing);
                if (At<uint8_t>(thing, 2) == 3) {
                    const RideInfo* info = reinterpret_cast<CObject*>(thing)->GetDBInfo();
                    if (At<int32_t>(const_cast<RideInfo*>(info), 0x118) == 0) return reinterpret_cast<CObject*>(thing);
                }
                id = At<uint16_t>(thing, 0xa);
            }
        }
    }
    return nullptr;
}

// 0x10032838 (356 bytes): when the camera stands over an entrance's thing, moves it to the middle of the thing's cell (10 units a cell, plus
// 5) and turns it to the cell's direction (bit 0: pi, bit 2: pi / 2, bit 4: 0, bit 6: 3 pi / 2; the other bits leave the angle).
void MoveCameraOverRideExit() {
    CObject* const object = GetThingAtPosnIfEntrance(gCameraPosition[0], gCameraPosition[2]);
    if (object == nullptr) return;
    const uint16_t cellIndex = At<uint16_t>(object, 0x34);
    const int32_t column = static_cast<int32_t>((static_cast<int32_t>(cellIndex) - 1) & 0x7f);
    const int32_t row = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(cellIndex) - 1) >> 7);
    const float x = ppc_fmadds(10.0f, static_cast<float>(column), 5.0f);
    const float z = ppc_fmadds(10.0f, static_cast<float>(row), 5.0f);
    gCameraPosition[0] = x;
    gCameraPosition[2] = z;
    char* const map = reinterpret_cast<char*>(gGameWorldRef) + 0x2d8;
    const CMapCell* const cell = reinterpret_cast<const CMapCell*>(map + (static_cast<int32_t>(cellIndex) - 1) * 0x44);
    switch (cell->GetDirection() & 0x55) {
        case 0x10: gCameraF5BF4 = FloatFromBits(0x00000000); break;
        case 0x04: gCameraF5BF4 = FloatFromBits(0x3fc90fdb); break;
        case 0x01: gCameraF5BF4 = FloatFromBits(0x40490fdb); break;
        case 0x40: gCameraF5BF4 = FloatFromBits(0x4096cbe4); break;
        default: break;
    }
}

// 0x1002ef00 (100 bytes): gives back every track piece of the ride (and each piece's links): the live bit goes down, the list counts at
// +0x1c (links) and +0x2c (pieces) of the collision world go down.
void FreeTrackForRide(tagBUMPRIDE* ride) {
    char* piece;
    while ((piece = At<Ptr32<char>>(ride, 0xbc).get()) != nullptr) {
        char* link;
        while ((link = At<Ptr32<char>>(piece, 0x1c).get()) != nullptr) {
            At<Ptr32<char>>(piece, 0x1c).mOffset = At<Ptr32<char>>(link, 0x14).mOffset;
            At<int32_t>(&gColList, 0x1c) -= 1;
            At<uint32_t>(link, 0) &= ~1u;
        }
        At<Ptr32<char>>(ride, 0xbc).mOffset = At<Ptr32<char>>(piece, 0x24).mOffset;
        At<uint32_t>(piece, 0) &= ~1u;
        At<int32_t>(&gColList, 0x2c) -= 1;
    }
}

// 0x1002edfc (216 bytes): the first free (live bit clear) of the 0x800 collision objects becomes a live object 0x1f at (x, y) with the radius
// twice; none free (or no table) is traced and answers NULL.
tabCOLOBJECT* BMP_StartColObject(int32_t x, int32_t y, int32_t radius) {
    tabCOLOBJECT* object = gColList.first.get();
    bool found = false;
    if (object != nullptr) {
        for (int32_t i = 0; i < 0x800; i++) {
            if ((object->flags & 1) == 0) {
                found = true;
                break;
            }
            object++;
        }
    }
    if (!found) {
        TRACE("Out of Collision Objects \n");
        return nullptr;
    }
    gColList.count += 1;
    object->flags = 0x1f;
    object->x = x;
    object->y = y;
    object->radiusY = radius;
    object->radiusX = radius;
    object->pad14[0] = 0;
    object->pad14[1] = 0;
    object->pad14[2] = 0;
    return object;
}

// 0x1002ed74 (72 bytes): starts an object and pushes it on the ride's list (ride +0xc0, the link at the object's +0x14). Like the original it
// does not look at a NULL answer.
void BMP_StartColObjectForRide(tagBUMPRIDE* ride, int32_t x, int32_t y, int32_t shape) {
    tabCOLOBJECT* object = BMP_StartColObject(x, y, shape);
    At<uint32_t>(object, 0x14) = At<uint32_t>(ride, 0xc0);
    At<Ptr32<tabCOLOBJECT>>(ride, 0xc0) = object;
}
