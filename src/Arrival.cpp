// CArrival (main app of SimTheme Park 2.04b3, Arrival.cpp; unit w113): the constructor.
#include "Arrival.h"

// 0x100f1ff4 (40 bytes), a leaf. The stores come in the order +4, +0x11, +0, +8, +0x10, +0xc; bytes +0x12 and +0x13 are not written.
CArrival::CArrival() {
    mF04 = 0;
    mGatesOpen = 1;
    mArrivalRate = 0;
    mTargetVehicleCapacity = 5;
    mOffloading = 0;
    mPeopleOnBus = 0;
}
