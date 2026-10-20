// Unit v5369: the trains' capacity and activation (TPConfig.cpp range of the main app).
//   RecalcDistancesFromCentreCar 0x1004a4e8, SetTrainCapacity 0x1004a328, ActivateCoasterTrain 0x1004a8a0
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

namespace {

void StoreFloat(uint32_t& word, float value) { std::memcpy(&word, &value, sizeof word); }

}  // namespace

// 0x1004a4e8 (260 bytes): the centre car is at distance 0; the cars before it count up, the cars after it count down (negative), by the type's
// spacing (first car, last car, or a middle car's); mF3C is the distance divided by the instance's mFAC.
void RecalcDistancesFromCentreCar(tag_sCoasterInstance* instance, sTrainInstance* train) {
    const tag_sCoasterType* type = train->mType;
    sCarInstance* centre = &train->mCars[train->mCentreCar];
    float zero = 0.0f;
    float distance = zero;
    centre->mF38 = zero;
    StoreFloat(centre->mF3C, zero);
    if (train->mCentreCar != 0) {
        for (int32_t i = static_cast<int32_t>(train->mCentreCar) - 1; i >= 0; --i) {
            sCarInstance* car = &train->mCars[i];
            float step;
            if (car->mIndex == 0) step = type->mF30C;
            else if (car->mIndex == train->mCarsInUse - 1) step = type->mF314;
            else step = type->mF310;
            distance = distance + step;
            car->mF38 = distance;
            StoreFloat(car->mF3C, distance / instance->mFAC);
        }
    }
    distance = 0.0f;
    for (int32_t i = static_cast<int32_t>(train->mCentreCar) + 1; i < static_cast<int32_t>(train->mCarsInUse); ++i) {
        sCarInstance* car = &train->mCars[i];
        float step;
        if (car->mIndex == 0) step = type->mF30C;
        else if (car->mIndex == train->mCarsInUse - 1) step = type->mF314;
        else step = type->mF310;
        distance = distance - step;
        car->mF38 = distance;
        StoreFloat(car->mF3C, distance / instance->mFAC);
    }
}

// 0x1004a328 (380 bytes). The word at +0xf0 of the instance (inside mFF0) is the capacity; +0xb8 (inside mQueueB0) gets its copy at the end.
void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c) {
    sTrainInstance* train = instance->mTrains;
    const tag_sCoasterType* instanceType = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    for (uint32_t t = 0; t < static_cast<uint32_t>(instanceType->mF2E8); ++t, ++train) {
        if ((train->mFlags & 4) != 0 || (b != 0 && b != train->mCarsInUse)) {
            if (b != 0) train->mCarsInUse = b;
            train->mF08 = 0;
            train->mCentreCar = ((train->mCarsInUse - 1) >> 1) + train->mF08;
            if (train->mCarsInUse != 0) {
                RecalcDistancesFromCentreCar(instance, train);
                GetTrainFrontAndRearSamples(instance, train);
            }
        }
        sCarInstance* car = train->mCars;
        if ((c & 1) != 0) {
            if (a == 0) a = train->mType->mF2F8;
            std::memcpy(instance->mFF0, &a, 4);
            uint32_t remaining = a;
            const tag_sCoasterType* type = train->mType;
            uint32_t slots = train->mCarsInUse;
            if (type->mCarTypeA != type->mCarTypeB) slots = slots - 1;
            if (type->mCarTypeC != type->mCarTypeB) slots = slots - 1;
            uint32_t used = 0;
            for (uint32_t i = 0; i < train->mCarsInUse; ++i, ++car) {
                if (car->mType == train->mType->mCarTypeB) {
                    uint32_t divisor = slots - used;
                    ++used;
                    uint32_t share = divisor != 0 ? remaining / divisor : remaining;   // divwu by 0 is undefined; the PowerPC emulator answers the dividend
                    car->mF44 = share;
                    remaining = remaining - share;
                } else {
                    car->mF44 = 0;
                }
            }
        }
    }
    std::memcpy(instance->mQueueB0.mF08, instance->mFF0, 4);
}

// 0x1004a8a0 (380 bytes): only when the instance's word +0x3c has bit 4.
void ActivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    if ((instance->mBlock3C & 4) == 0) return;
    instance->mFlags |= 0x800;
    GetRideAnimTime(0);
    train->mFlags |= 2;
    sCarInstance* car = train->mCars;
    for (uint32_t i = 0; i < train->mCarsInUse; ++i, ++car) car->mF1C = 0;
    car = train->mCars + (train->mCarsInUse - 1);
    for (uint32_t i = 0; i < train->mCarsInUse; ++i, --car) {
        car->mFlags |= 1;
        if (car->mRide != 0) {
            tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
            if ((set->mFlags & 0x4000) != 0) EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            else EnableMeshDraw(set->mHeads);
        }
        car->mCurrentA = car->mHeadsA;
        car->mCurrentB = car->mHeadsB;
        car->mCountA = 0;
        car->mCountB = 0;
        if ((car->mType->mFlags & 1) == 0 && car->mRide != 0)
            SetCurRideAnim(gRideAnimSets[car->mRide], 5, 0, 1, 1.0f, 0);
    }
    StartTrainSounds(instance, train, 0);
}
