// CArrival (main app of SimTheme Park 2.04b3, Arrival.cpp; unit w129): Init, Update and UpdateArrivalRate.
#include "Arrival.h"

// 0x100f22c4 (4 bytes): a bare return.
void CArrival::Init() {}

// 0x100f22ec (60 bytes): the rate first, then the people (CreatePeople 0x100f2350 is untranslated).
void CArrival::Update() {
    UpdateArrivalRate();
    CreatePeople();
}

// 0x100f2600 (4 bytes, a companion of the unit): a bare return.
void CArrival::UpdateArrivalRate() {}
