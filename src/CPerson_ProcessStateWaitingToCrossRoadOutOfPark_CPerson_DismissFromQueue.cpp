// Unit v5150: a person in a ride's queue (CPerson::DismissFromQueue, EvaluateQueueEdit, FindQueueDestination, ExitRide).
// The person and the ride are reached by Mac offset.
#include <cstddef>
#include <cstdint>

#include "CellBase.h"
#include "Debug.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

namespace {

template <class T>
T* At(void* base, uint32_t offset) {
    return reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";                    // 0x10244aa8
const char kNoSetDest[] = "QQQ - FindQueueDestination SetDest failed, so I'm standing in queue\n";                 // 0x10245198
const char kDismissed[] = "Person %d: dismissed from queue on ride %d\n";                                          // 0x10245210
const char kEdited[] = "Person %d: Queue edited underneath me while Standing In Queue (I'm in state %d)\n";        // 0x1024523c
const char kShortened[] = "Person %d: The queue was shortened and there's no room for me any more\n";              // 0x1024528d
const char kLeaving[] = "Person %d: ExitRide, leaving ride %d";                                                    // 0x102452d5
const char kBlocked[] = "Person %d: SetDest on ride exit failed, exit of object %d is blocked!\n";                 // 0x102452fa

uint32_t ThingIdOf(CPerson* self) {
    return reinterpret_cast<CThing*>(self)->GetThingId().mIndex;
}

CObject* RideRow(uint16_t index) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return static_cast<CObject*>(gThingArray[index].mThing);
}

CMapCell* CellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + (static_cast<int32_t>(id) - 1) * 0x44);
}

// The common tail of DismissFromQueue and EvaluateQueueEdit (the same code after the trace).
void LeaveQueueTail(CPerson* self) {
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(At<uint8_t>(self, 0x30));
    thoughts->RecordEvent(static_cast<CThoughts::EventType>(6), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{*At<uint16_t>(self, 0x1dc)});
    uint16_t id = ThingIdOf(self);
    if ((id & 7) == 0) reinterpret_cast<CPeep*>(self)->PlaySound(gSoundBanks[1], 0x80);
    self->LowerHappiness(static_cast<CPerson::Amount>(1));
    *At<uint16_t>(self, 0x22a) = 0;
    *At<uint16_t>(self, 0x228) = 0;
    *At<int32_t>(self, 0x1f8) = 0;
    *At<uint16_t>(self, 0x1dc) = 0;
    *At<uint8_t>(self, 0x1f1) = 0;
    self->SetState(static_cast<CPerson::PersonState>(6));
}

}  // namespace

// 0x1011f1b8
void CPerson::DismissFromQueue() {
    uint16_t ride = *At<uint16_t>(this, 0x1dc);
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kDismissed, static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
    LeaveQueueTail(this);
}

// 0x1011f2f8
void CPerson::EvaluateQueueEdit() {
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kEdited, static_cast<uint32_t>(id), *At<int32_t>(this, 0x220));
    uint16_t rideIndex = *At<uint16_t>(this, 0x1dc);
    CObject* ride = RideRow(rideIndex);
    int32_t length = *At<int32_t>(ride, 0x3c);
    uint32_t position = static_cast<uint32_t>(ride->GetPositionInQueue(reinterpret_cast<CThing*>(this)->GetThingId()));
    if (position < static_cast<uint32_t>(length << 2)) return;
    if (*At<int32_t>(this, 0x220) == 0xe) return;
    uint16_t id2 = ThingIdOf(this);
    CTRACE(2, kShortened, static_cast<uint32_t>(id2));
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(*At<int32_t>(this, 0xc), &x, &y, &z, &mode);
    uint16_t id3 = ThingIdOf(this);
    if (id3 % 3 == 0) reinterpret_cast<CThoughts*>(At<uint8_t>(this, 0x30))->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xd), false);
    ride->RemovePersonFromQueue(reinterpret_cast<CThing*>(this)->GetThingId());
    uint16_t rideId = *At<uint16_t>(this, 0x1dc);
    uint16_t id4 = ThingIdOf(this);
    CTRACE(2, kDismissed, static_cast<uint32_t>(id4), static_cast<uint32_t>(rideId));
    LeaveQueueTail(this);
    *At<uint16_t>(this, 0x1dc) = 0;
    SetState(static_cast<PersonState>(6));
}

// 0x1011ef14
int32_t CPerson::FindQueueDestination() {
    uint16_t rideIndex = *At<uint16_t>(this, 0x1dc);
    CObject* ride = RideRow(rideIndex);
    int32_t position = ride->GetPositionInQueue(reinterpret_cast<CThing*>(this)->GetThingId());
    if (position == -1) return 0;
    *At<uint8_t>(this, 0x1f1) = static_cast<uint8_t>(position);
    uint16_t rideIndex2 = *At<uint16_t>(this, 0x1dc);
    CObject* ride2 = RideRow(rideIndex2);
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell{0};
    uint8_t x = 0, y = 0;
    ride2->ConvertQPositionToMapPos(static_cast<uint32_t>(position), &cell, &x, &y);
    uint32_t h = cell.mIndex;
    MapPos a{static_cast<uint8_t>((h - 1) >> 7), y};
    MapPos b{static_cast<uint8_t>((h - 1) & 0x7f), x};
    if (reinterpret_cast<CPeep*>(this)->SetDest(b, a)) {
        SetState(static_cast<PersonState>(0xc));
        return 1;
    }
    CTRACE(2, kNoSetDest);
    return 0;
}

// 0x1011f5a4
int32_t CPerson::ExitRide() {
    uint16_t rideIndex = *At<uint16_t>(this, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CObject* ride = static_cast<CObject*>(gThingArray[rideIndex].mThing);
    uint16_t rideId = *At<uint16_t>(this, 0x1dc);
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kLeaving, static_cast<uint32_t>(id), static_cast<uint32_t>(rideId));
    MapPos first{}, second{};
    ride->GetStandPoint(1, &first, &second);
    reinterpret_cast<CPeep*>(this)->Teleport(first, second);
    uint16_t entry = *At<uint16_t>(ride, 0x34);
    CMapCell* cell = CellOfId(entry);
    uint8_t direction;
    if (*At<uint16_t>(ride, 0x34) == *At<uint16_t>(ride, 0x32)) {
        direction = OPPOSITE(cell->GetDirection());
    } else {
        direction = cell->GetDirection();
    }
    CMapCell* neighbour = cell->GetNeighbouringCell(direction);
    uint16_t next = *reinterpret_cast<uint16_t*>(neighbour);
    if (!CellOfId(next)->IsQueue() &&
        reinterpret_cast<CPeep*>(this)->SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{next})) {
        UseObject(ride);
        SetState(static_cast<PersonState>(0xf));
        return 1;
    }
    uint16_t rideId2 = *At<uint16_t>(this, 0x1dc);
    uint16_t id2 = ThingIdOf(this);
    CTRACE(2, kBlocked, static_cast<uint32_t>(id2), static_cast<uint32_t>(rideId2));
    ride->NotifyExitBlocked();
    return 0;
}
