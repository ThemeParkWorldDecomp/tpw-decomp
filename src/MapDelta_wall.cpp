// MapDelta.cpp (main app of SimTheme Park 2.04b3), unit w141: MAP_wall_in_way (0x10101ca4, 1,724 bytes).
//
// Whether a wall (or anything that stops the walk) stands between the square (x, z) and its neighbour in direction dir (0 and 2 along z,
// 1 and 3 along x). The cells are read by raw offsets from the world pointer as src/MapDelta_map.cpp does: map cells 0x44 bytes at world
// + 0x2d8, track cells 0x28 bytes at +0x110000 of that; ids are 1 + x + (z << 7). The predicates are the untranslated cell members, asked
// in the listing's order (each call counts). The edges give 1 first. dir 4 or more leaves the neighbour's id uninitialised on the Mac
// (the stack halfword): 0 here.
#include <cstdint>

#include "Debug.h"
#include "MapCell.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

uint16_t gEntranceWalkEnd;   // 0x102a0af0, BSS 0

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

const CMapCell* MapCellAt(uint16_t id) {
    unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
    return reinterpret_cast<const CMapCell*>(base + (static_cast<int32_t>(id) - 1) * 0x44);
}

// The mask of GetNeighbours' byte that blocks a step in direction dir (the else arm is dir 3).
int32_t NeighbourBlocked(uint8_t neighbours, uint8_t dir) {
    const int32_t mask = dir == 0 ? 0x10 : dir == 2 ? 1 : dir == 1 ? 0x40 : 4;
    return (neighbours & mask) == 0;
}
}  // namespace

int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    if (x == 0 && dir == 3) return 1;
    if (z == 0 && dir == 0) return 1;
    if (x == 0x7f && dir == 1) return 1;
    if (z == 0x7f && dir == 2) return 1;
    const int32_t m = static_cast<int32_t>(mode);

    const uint16_t thisId = static_cast<uint16_t>(x + z * 128 + 1);
    uint16_t thatId = 0;
    if (dir == 0) thatId = static_cast<uint16_t>((z - 1) * 128 + 1 + x);
    else if (dir == 1) thatId = static_cast<uint16_t>(x + z * 128 + 2);
    else if (dir == 2) thatId = static_cast<uint16_t>((z + 1) * 128 + 1 + x);
    else if (dir == 3) thatId = static_cast<uint16_t>(x + z * 128);

    const CMapCell* const here = MapCellAt(thisId);
    const CMapCell* const that = MapCellAt(thatId);

    if (that->IsPath() != 0) {
        if (here->IsQueue() == 0) return 0;
    }
    if (here->IsRide() != 0) {
        if (that->IsRide() != 0) return 0;
    }
    if (that->IsRide() != 0) return 1;
    if (that->IsWalkable() != 0 && m != 2) return 0;
    if (m == 2) {
        if (that->IsWalkableInFirstPerson() != 0) return 0;
        if (that->IsEntrance() != 0) {
            const CMapCell* const entrance = MapCellAt(that->mParentID);
            if (entrance != nullptr) {
                uint16_t id = entrance->mWho;
                while (id != gEntranceWalkEnd) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                    CThing* const thing = static_cast<CThing*>(gThingArray[id].mThing);
                    if (thing->mKind == 3) {
                        CObject* const object = reinterpret_cast<CObject*>(thing);
                        if (IsRideRideable(object, object->mF1C) != 0) {
                            if (object->GetDBInfo()->Value(kRideSlot_CannotRide).mInteger == 0) return 0;
                        }
                        return 1;
                    }
                    id = thing->mCellNext;
                }
            }
        }
    }
    if (that->IsNoGo() != 0) return 1;
    if (that->IsWater() != 0) return 1;
    if (that->IsQueue() != 0) {
        if (here->IsPath() != 0 || here->IsQueue() != 0) return NeighbourBlocked(that->GetNeighbours(), dir);
        return 1;
    }
    if (here->IsQueue() != 0 && that->IsPath() != 0) return NeighbourBlocked(that->GetNeighbours(), dir);
    if (that->IsTrack() != 0) {
        uint16_t trackId = 0;
        if (that->IsTrackX() != 0) trackId = that->GetTrackCellEquiv()->mParentID;
        else trackId = that->mCellId;
        unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
        const CTrackCell* const track = reinterpret_cast<const CTrackCell*>(base + 0x110000 + (static_cast<int32_t>(trackId) - 1) * 0x28);
        if (track->IsBridge() == 0) return 1;
    }
    if (here->IsPath() == 0 && here->IsQueue() == 0) return 0;
    if (m == 1 && here->IsPath() != 0 && that->IsPath() == 0 && that->IsQueue() == 0 && that->IsRide() == 0) return 0;
    if (m == 2 && here->IsPath() != 0 && that->IsGrass() != 0) return 0;
    if ((here->IsExit() != 0 || here->IsEntrance() != 0) && that->IsGrass() != 0) {
        if (dir == 3 && here->GetDirection() == 4) return 0;
        if (dir == 1 && here->GetDirection() == 0x40) return 0;
        if (dir == 0 && here->GetDirection() == 0x10) return 0;
        if (dir == 2 && here->GetDirection() == 1) return 0;
    }
    return 1;
}
