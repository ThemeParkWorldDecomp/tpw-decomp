// CArrival (Arrival.cpp, main app): the park's visitor arrival settings, 0x14 bytes (unit w113: the constructor). Names are the
// serialiser's (CArrival::Serialise 0x100f2040).
#pragma once
#include <cstdint>

class CArrival {
public:
    CArrival();    // 0x100f1ff4 (40 bytes, src/game/people/Arrival.cpp)

    // Unit w129 (src/game/people/Arrival.cpp).
    void Init();               // 0x100f22c4 (4 bytes): a bare return
    void Update();             // 0x100f22ec (60 bytes): UpdateArrivalRate, then CreatePeople
    void UpdateArrivalRate();  // 0x100f2600 (4 bytes): a bare return
    // Unit w176 (src/game/people/Arrival.cpp). 0x100f2350 (644 bytes): starts a bus when the time since the last one is above the balance's TimeBetweenArrivals,
    // drops its people off one per turn, sends the vehicle on its next leg.
    void CreatePeople();
    // 0x100f2638 (472 bytes): how many people the next bus carries, from the park's attraction points and visitors (capped at 500 online, 1500 offline).
    int32_t HowManyPeople();
    // 0x100f2840 (216 bytes): makes one visitor (a CPerson at the bus stop); always 1.
    int32_t CreatePerson();

    int32_t mArrivalRate;             // 0x00  a long (cleared)
    int32_t mF04;                     // 0x04  a CTimeSig (the time of the last bus; serialised by CTimeSig::Serialise)
    int32_t mTargetVehicleCapacity;   // 0x08  a long (5)
    int32_t mPeopleOnBus;             // 0x0c  (cleared)
    uint8_t mOffloading;              // 0x10  (cleared)
    uint8_t mGatesOpen;               // 0x11  (1)
    uint8_t mF12[2];                  // 0x12  never written

    // Unit w146, translated in unit w176 (src/game/people/Arrival.cpp): 0x100f2040 (596 bytes), the settings; 0 on success, 1 on failure.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
};
static_assert(sizeof(CArrival) == 0x14, "CArrival is 0x14 bytes");
