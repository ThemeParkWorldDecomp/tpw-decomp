// CPerson: the three states of a guest leaving the park (unit v5006):
//   ProcessStateLeavingPark 0x1011e2e8, ProcessStateWaitingToCrossRoadOutOfPark 0x1011e42c, ProcessStateWaitingForBusOuttaHere 0x1011e654.
#include <cstdint>

#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

namespace {

template <class T>
T* At(void* base, uint32_t offset) {
    return reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

// fctiwz: truncates, saturating (the low word is what the listing keeps).
int32_t Fctiwz(float f) {
    if (f != f) return INT32_MIN;
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f < -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}

// The four cells around the bus stop: the stop, one to the east, two rows back, and that one to the east.
void BusStopCells(uint16_t* out) {
    out[0] = gWorldMap->GetBusStopPos(0).mIndex;
    out[1] = static_cast<uint16_t>(1 + (0 << 7) + gWorldMap->GetBusStopPos(0).mIndex);
    out[2] = static_cast<uint16_t>(0 - 256 + gWorldMap->GetBusStopPos(0).mIndex);
    out[3] = static_cast<uint16_t>(1 - 256 + gWorldMap->GetBusStopPos(0).mIndex);
}

uint16_t MyCell(CPerson* self) {
    uint32_t x = *At<uint8_t>(self, 4);
    uint32_t y = *At<uint8_t>(self, 6);
    return static_cast<uint16_t>(x + (y << 7) + 1);
}

void WalkTo(CPerson* self, uint16_t dest) {
    uint8_t a = static_cast<uint8_t>(gWorld->GetRand());
    uint8_t b = static_cast<uint8_t>(gWorld->GetRand());
    uint32_t m = static_cast<uint32_t>(dest) - 1;
    MapPos x;
    MapPos y;
    x.mCell = static_cast<uint8_t>(m & 0x7f);
    x.mOffset = a;
    y.mCell = static_cast<uint8_t>(m >> 7);
    y.mOffset = b;
    reinterpret_cast<CPeep*>(self)->SetDest(x, y);
    self->SetState(static_cast<CPerson::PersonState>(0x14));
}

}  // namespace

// 0x1011e2e8
void CPerson::ProcessStateLeavingPark() {
    if (*At<int32_t>(this, 0x1bc) > 0 && (Fctiwz(*At<float>(this, 0x19c)) & 0xff) != 0 && !gWorld->IsParkClosed() &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark()) {
        CTRACE(2, "Make up your mind!\n");
        *At<int32_t>(this, 0x188) = 0;
        SetState(static_cast<PersonState>(6));
        return;
    }
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        SetState(static_cast<PersonState>(0x13));
    } else if (walks == 2) {
        CTRACE(2, "I'm stuck in the park, even though it's closed!!");
        *At<int32_t>(this, 0x188) = 0;
        SetState(static_cast<PersonState>(6));
    }
}

// 0x1011e42c
void CPerson::ProcessStateWaitingToCrossRoadOutOfPark() {
    uint16_t cells[4];
    BusStopCells(cells);
    if (gWorld->ArrivalVehicle_CrossingIsClear()) {
        uint16_t dest = cells[gWorld->GetRand() & 3];
        WalkTo(this, dest);
    }
}

// 0x1011e654
void CPerson::ProcessStateWaitingForBusOuttaHere() {
    int32_t status = gWorld->ArrivalVehicle_GetStatus();
    if (status == 4) {
        uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        uint16_t cell = MyCell(this);
        uint16_t who = reinterpret_cast<CMapCell*>(gWorldMap)[cell - 1].mWho;
        if (who == id) {
            reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
            reinterpret_cast<CThing*>(this)->KillMe();
        }
    } else if (static_cast<uint32_t>(status - 1) <= 2) {
        uint16_t cells[4];
        BusStopCells(cells);
        uint16_t dest;
        if (gWorld->ArrivalVehicle_IsSeagoing()) {
            if (MyCell(this) == cells[2]) return;
            if (MyCell(this) == cells[3]) return;
            dest = cells[2 + (gWorld->GetRand() & 1)];
        } else {
            if (MyCell(this) == cells[0]) return;
            if (MyCell(this) == cells[1]) return;
            dest = cells[gWorld->GetRand() & 1];
        }
        WalkTo(this, dest);
    }
}
