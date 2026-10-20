// CObject, main app, unit w197 (the Mac files them under Object.cpp, address range 0x10108a28 to 0x1010a488; the other CObject members are in
// src/Object.cpp and src/Object_turn.cpp): the ride's queue. Fifteen members: the list of queued people (AddPersonToQueue, RemovePersonFromQueue,
// GetNumPeopleInQueue, GetPositionInQueue), the line of queue cells (GetFrontOfPhysicalQueue, GetBackOfPhysicalQueue, GetBackOfVirtualQueue,
// GetBackOfQueue, GetQCellBehind, RecalculateBackOfQueue, IsBOQConnected, IsExitCellConnected) and the queue positions turned into map positions
// (ConvertQPositionToMapPos, ConvertPhysicalQPositionToMapPos, ConvertVirtualQPositionToMapPos). CPerson is a placeholder class here, so the two
// link halfwords of a person (+0x228 the person behind, +0x22a the person ahead) are read through file-local accessors, as src/Object_turn.cpp does.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPCamera.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ppc_math.h"
#include "rsse.h"

// CPerson::EvaluateQueueEdit (0x1011f2f8, untranslated), by its Mac name as src/Object_turn.cpp does for its CPerson members.
void ObjectEvaluateQueueEdit(CPerson*) __asm__("__ZN7CPerson17EvaluateQueueEditEv");

namespace {
using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x3cd of the string pool at 0x1024294c
constexpr const char* kNotRemoved = "Person not successfully removed from queue\n";                        // +0x439
constexpr uint16_t kNoThing = 0;   // the halfword at 0x102a0be2 and 0x102a0be4 (also the "no cell" halfword at 0x102a0be0 and the like), all 0
constexpr std::size_t kBehind = 0x228;   // a person's link to the person behind in the queue
constexpr std::size_t kAhead = 0x22a;    // a person's link to the person ahead in the queue
constexpr uint32_t kCellSize = 0x44;

uint16_t HalfAt(const void* base, std::size_t offset) {
    uint16_t half;
    std::memcpy(&half, static_cast<const char*>(base) + offset, sizeof(half));
    return half;
}

void SetHalfAt(void* base, std::size_t offset, uint16_t value) { std::memcpy(static_cast<char*>(base) + offset, &value, sizeof(value)); }

// The thing table's pointer for an id; every dereference of one is preceded by the same game assertion on the table's first row.
char* Guest(uint16_t id) { return static_cast<char*>(gThingArray[id].mThing); }
void CheckRowZero() { GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero); }

CMapCell* CellAt(CMap* map, int32_t id) { return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(map) + (id - 1) * static_cast<int32_t>(kCellSize)); }

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint8_t PositionByte(uint32_t position) {
    const float quarter = static_cast<float>(position) * 0.25f;
    const float scaled = 255.0f * quarter;
    return static_cast<uint8_t>(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled))) & 0xff);
}

constexpr uint8_t kDirections[4] = {1, 4, 0x10, 0x40};
}  // namespace

// Appends the person to the end of the list (the head is mQueuedPerson, each person links to the one behind at +0x228) and links the person back to
// the one that was last (+0x22a; 0 when the queue was empty).
void CObject::AddPersonToQueue(ThingTag person) {
    CTRACE(2, "Object %d adding person %d to queue\n", mBase.GetThingId().mIndex, person.mIndex);
    uint16_t last = mQueuedPerson;
    if (last == kNoThing) {
        mQueuedPerson = person.mIndex;
    } else {
        for (;;) {
            CheckRowZero();
            if (HalfAt(Guest(last), kBehind) == kNoThing) break;
            CheckRowZero();
            last = HalfAt(Guest(last), kBehind);
        }
        CheckRowZero();
        SetHalfAt(Guest(last), kBehind, person.mIndex);
    }
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kAhead, last);
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kBehind, kNoThing);
}

// Takes the person out of the list (the neighbours link to each other, or the head moves on), clears the ride script's variable 0 when it names the
// person, and asserts that both of the person's links are clear.
void CObject::RemovePersonFromQueue(ThingTag person) {
    CTRACE(2, "Object %d removing person %d from queue\n", mBase.GetThingId().mIndex, person.mIndex);
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (static_cast<uint32_t>(RSSE_Getvar(script, 0)) == person.mIndex) RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 0);
    CheckRowZero();
    const uint16_t ahead = HalfAt(Guest(person.mIndex), kAhead);
    CheckRowZero();
    const uint16_t behind = HalfAt(Guest(person.mIndex), kBehind);
    if (ahead == kNoThing) {
        mQueuedPerson = behind;
    } else {
        CheckRowZero();
        SetHalfAt(Guest(ahead), kBehind, behind);
    }
    if (behind != kNoThing) {
        CheckRowZero();
        SetHalfAt(Guest(behind), kAhead, ahead);
    }
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kAhead, kNoThing);
    CheckRowZero();
    SetHalfAt(Guest(person.mIndex), kBehind, kNoThing);
    CheckRowZero();
    GAME_ASSERTMSG(kNoThing == HalfAt(Guest(person.mIndex), kBehind), kNotRemoved);
    CheckRowZero();
    GAME_ASSERTMSG(kNoThing == HalfAt(Guest(person.mIndex), kAhead), kNotRemoved);
}

// The length of the list: the position of nobody.
int32_t CObject::GetNumPeopleInQueue() const { return GetPositionInQueue(ThingTag{kNoThing}); }

// The position of the person in the list (0 for the head), or the length of the list for nobody. The walk follows the +0x228 links of people who are
// in the queueing state; a person who is not (or a thing that is gone) ends it. A person who is not found is logged, asserted and answered -1.
// A row with no thing leaves the walk where it is (the original loops for ever; kept).
int32_t CObject::GetPositionInQueue(ThingTag person) const {
    int32_t count = 0;
    uint16_t current = mQueuedPerson;
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(&mBase);
    while (current != person.mIndex && current != kNoThing) {
        count++;
        savvy->GetWorld();
        CheckRowZero();
        CPerson* guest = reinterpret_cast<CPerson*>(Guest(current));
        if (guest != nullptr && guest->InQueueingState() != 0) {
            current = HalfAt(guest, kBehind);
        } else if (guest != nullptr) {
            current = kNoThing;
        }
    }
    if (current == kNoThing && person.mIndex != kNoThing) {
        const uint32_t turn = savvy->GetWorld().mTurn;
        CTRACE(2, "Object %d (type %d): Could not find person %d in the queue (%d)\n", mBase.GetThingId().mIndex, mObjectType, person.mIndex, turn);
        GAME_ASSERTMSG(0, "GetPositionInQueue: Could not find the thing in the queue\n");
        count = -1;
    }
    return count;
}

// The cell in front of the queue: the neighbour of the entry cell (mF32) in the first direction (up, down, left, right) it connects to, 0 when it
// connects to none.
MapTag CObject::GetFrontOfPhysicalQueue() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(&mBase);
    CMapCell* cell = CellAt(savvy->GetMap(), mF32);
    const uint8_t neighbours = cell->GetNeighbours();
    const MapDelta* delta;
    if ((neighbours & 1) != 0) {
        delta = &gMapDeltaMinusY;
    } else if ((neighbours & 0x10) != 0) {
        delta = &gMapDeltaPlusY;
    } else if ((neighbours & 0x40) != 0) {
        delta = &gMapDeltaMinusX;
    } else if ((neighbours & 4) != 0) {
        delta = &gMapDeltaPlusX;
    } else {
        return MapTag{kNoThing};
    }
    return MapTag{static_cast<uint16_t>(mF32 + delta->mX + static_cast<int32_t>(static_cast<uint32_t>(delta->mY) << 7))};
}

// The last cell of the queue's line (the cache mF36, 0 when stale): walks back from the front cell, at most 1000 cells, counting them in mF3C. A line
// that long is logged as a crash and answered 0.
MapTag CObject::GetBackOfPhysicalQueue() {
    int32_t counter = 1000;
    if (mF36 == kNoThing) {
        const MapTag front = GetFrontOfPhysicalQueue();
        mF36 = front.mIndex;
        mF3C = 0;
        MapTag current = front;
        while (current.mIndex != kNoThing && (counter-- > 0)) {
            mF36 = current.mIndex;
            current = GetQCellBehind(current);
            mF3C = mF3C + 1;
        }
        if (counter < 1) {
            CTRACE(0xc, "*** GetBackOfQueue() crashed! ***\n");
            return MapTag{kNoThing};
        }
        return MapTag{mF36};
    }
    return MapTag{mF36};
}

// Both flag cases call the physical one.
MapTag CObject::GetBackOfVirtualQueue() { return GetBackOfPhysicalQueue(); }

MapTag CObject::GetBackOfQueue() {
    if ((mFlags & 8) != 0) return GetBackOfPhysicalQueue();
    return GetBackOfVirtualQueue();
}

// Forgets the cached back, finds it again, stamps the back cell on the map, tells everybody in the queue (except the one being loaded) to evaluate
// again, logs whether the back is connected, reopens a ride that was closed and can open, and clears mF5A.
void CObject::RecalculateBackOfQueue() {
    mF36 = kNoThing;
    GetBackOfPhysicalQueue();
    CTRACE(2, "Object's queue is now %d cells long\n", mF3C);
    CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8);
    const uint32_t stamp = map->MakeNewTimeStamp();
    const MapTag back = GetBackOfPhysicalQueue();
    map->SetTimeStamp(stamp, back);
    CTRACE(2, "Telling people in queue to reevaluate");
    uint16_t current = mQueuedPerson;
    while (current != kNoThing) {
        CheckRowZero();
        CPerson* guest = reinterpret_cast<CPerson*>(Guest(current));
        const uint16_t next = HalfAt(guest, kBehind);
        if (current != mPersonLoaded) ObjectEvaluateQueueEdit(guest);
        current = next;
    }
    const int32_t connected = IsBOQConnected();
    CTRACE(2, "Back of queue is %sconnected\n", connected != 0 ? "" : "not ");
    if (IsClosed() && IsOpenable() != 0 && (IsTrackRide() == 0 || mF28 != 0)) Reopen();
    mF5A = 0;
}

// Whether the back of the queue joins up: a queue with cells, when the back cell's direction differs from its neighbour byte; a queue without, when the
// entry cell and its neighbour across the ride's rotation connect to each other.
int32_t CObject::IsBOQConnected() {
    if ((mFlags & 8) != 0) {
        const MapTag back = GetBackOfPhysicalQueue();
        if (back.mIndex == kNoThing) return 0;
        CMapCell* cell = CellAt(gWorldMap, back.mIndex);
        return cell->GetDirection() != cell->GetNeighbours() ? 1 : 0;
    }
    CMapCell* cell = CellAt(gWorldMap, mF32);
    uint8_t direction = 0;   // the original leaves it unset for a rotation other than 0, 90, 180 and 270
    if (mRotation == 0) {
        direction = 0x10;
    } else if (mRotation == 90) {
        direction = 4;
    } else if (mRotation == 180) {
        direction = 1;
    } else if (mRotation == 270) {
        direction = 0x40;
    }
    const uint8_t opposite = OPPOSITE(direction);
    CMapCell* across = cell->GetNeighbouringCell(opposite);
    const bool joined = across->IsPath() != 0 && (across->GetNeighbours() & direction) != 0;
    return joined && (cell->GetNeighbours() & opposite) != 0 ? 1 : 0;
}

// Whether the exit cell (mF34) has a way out: from the neighbour across its direction (turned round when the exit is the entry), the first of the four
// sides that is not the opposite and has no wall in the way. The original mixes the direction bits (1, 4, 0x10, 0x40) with the index i.
int32_t CObject::IsExitCellConnected() {
    CMapCell* cell = CellAt(gWorldMap, mF34);
    uint8_t direction = cell->GetDirection();
    if (mF34 == mF32) direction = OPPOSITE(direction);
    CMapCell* next = cell->GetNeighbouringCell(direction);
    const int32_t id = next->mCellId;
    for (int32_t i = 0; i < 4; i++) {
        const uint8_t opposite = OPPOSITE(direction);
        if (kDirections[i] != opposite &&
            MAP_wall_in_way(static_cast<uint8_t>((id - 1) & 0x7f), static_cast<uint8_t>((static_cast<uint32_t>(id - 1) >> 7) & 0xff), static_cast<uint8_t>(i),
                            static_cast<NavMode>(0)) == 0) {
            CTRACE(2, "Connected in direction %d\n", i);
            return 1;
        }
    }
    return 0;
}

// The queue cell behind the cell: the first neighbour (up, down, right, left) that is a queue cell, not an entrance, whose direction is the one that
// leads to this side (0x10, 1, 0x40, 4), else 0. Reads the game world's map; no `this`.
MapTag CObject::GetQCellBehind(MapTag cell) {
    CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8);
    CMapCell* here = CellAt(map, cell.mIndex);
    const MapDelta* deltas[4] = {&gMapDeltaMinusY, &gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaMinusX};
    const uint8_t wanted[4] = {0x10, 1, 0x40, 4};
    for (int32_t i = 0; i < 4; i++) {
        MapDelta delta = *deltas[i];
        CMapCell* neighbour = here->GetNeighbouringCell(&delta, 1);
        if (neighbour != nullptr && neighbour->IsQueue() != 0 && neighbour->IsEntrance() == 0 && neighbour->GetDirection() == wanted[i]) {
            return MapTag{neighbour->mCellId};
        }
    }
    return MapTag{kNoThing};
}

// A queue position (0.25 of a cell each) to a map position: a ride with a queue line uses the physical conversion, the other the virtual one (positions
// 0 to 3, asserted).
int32_t CObject::ConvertQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    if ((mFlags & 8) != 0) return ConvertPhysicalQPositionToMapPos(position, cell, x, y);
    GAME_ASSERTMSG(position < 4 ? 1 : 0, "Virtual queue problem!\n");
    return ConvertVirtualQPositionToMapPos(position, cell, x, y);
}

// The cell is the queue cell `position / 4` back from the front (0 once the line ends); the place in it follows its direction: the byte
// 255 * position / 4 (of the remainder) along it and a random 0x72 to 0x8d across. In the second half of the cell the direction is the one of the cell
// behind (or, with none, the side of the first path neighbour, else the cell's own direction) and the byte is shifted by 0x80 for direction 4.
int32_t CObject::ConvertPhysicalQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    MapTag current = GetFrontOfPhysicalQueue();
    while (position > 3 && current.mIndex != kNoThing) {
        position = position - 4;
        current = GetQCellBehind(current);
    }
    cell->mIndex = current.mIndex;
    const uint8_t along = PositionByte(position);
    const uint32_t random = gWorld->GetRand();
    const uint8_t across = static_cast<uint8_t>(random % 0x1c + 0x72);
    if (along <= 0x80) {
        const uint8_t direction = CellAt(gWorldMap, current.mIndex)->GetDirection();
        if (direction == 0x10) {
            *x = across;
            *y = static_cast<uint8_t>(0xff - along);
        } else if (direction == 4) {
            *x = static_cast<uint8_t>(0xff - along);
            *y = across;
        } else if (direction == 1) {
            *x = across;
            *y = along;
        } else if (direction == 0x40) {
            *x = along;
            *y = across;
        } else {
            GAME_ASSERTMSG(0, "Dodgy cell direction\n");
        }
        return 0;
    }
    uint8_t direction = 0;
    const MapTag behind = GetQCellBehind(current);
    if (behind.mIndex == kNoThing) {
        CMapCell* here = CellAt(gWorldMap, current.mIndex);
        const uint8_t neighbours = here->GetNeighbours();
        bool found = false;
        int32_t opposite = 2;
        uint8_t sides[4] = {kDirections[0], kDirections[1], kDirections[2], kDirections[3]};
        for (int32_t i = 0; i < 4; i++) {
            const uint8_t side = sides[i];
            if ((neighbours & side) != 0 && (here->GetNeighbours() & side) != 0) {
                CMapCell* next = here->GetNeighbouringCell(side);
                if (next->IsPath() != 0) {
                    direction = sides[opposite];
                    found = true;
                    break;
                }
            }
            opposite = (i + 3) & 3;
        }
        if (!found) {
            CTRACE(0xc, "*** No path attached to end of queue! ***\n");
            direction = CellAt(gWorldMap, current.mIndex)->GetDirection();
        }
    } else {
        direction = CellAt(gWorldMap, behind.mIndex)->GetDirection();
    }
    if (direction == 0x10) {
        *x = across;
        *y = static_cast<uint8_t>(0xff - along);
    } else if (direction == 4) {
        *x = static_cast<uint8_t>(along + 0x80);
        *y = across;
    } else if (direction == 1) {
        *x = across;
        *y = along;
    } else if (direction == 0x40) {
        *x = along;
        *y = across;
    }
    return 0;
}

// The back of the queue cell, and the place across it by the entry cell's direction.
int32_t CObject::ConvertVirtualQPositionToMapPos(uint32_t position, MapTag* cell, uint8_t* x, uint8_t* y) {
    const uint8_t along = PositionByte(position);
    const uint32_t random = gWorld->GetRand();
    const uint8_t across = static_cast<uint8_t>(random % 0x1c + 0x72);
    const MapTag back = GetBackOfPhysicalQueue();
    cell->mIndex = back.mIndex;
    const uint16_t entry = mF32;
    CMap* map = gWorldMap;
    GetBackOfPhysicalQueue();
    const uint8_t direction = CellAt(map, entry)->GetDirection();
    if (direction == 0x10) {
        *x = across;
        *y = static_cast<uint8_t>(0xff - along);
    } else if (direction == 4) {
        *x = static_cast<uint8_t>(0xff - along);
        *y = across;
    } else if (direction == 1) {
        *x = across;
        *y = along;
    } else if (direction == 0x40) {
        *x = along;
        *y = across;
    } else {
        GAME_ASSERTMSG(0, "Dodgy cell direction\n");
    }
    return 0;
}
