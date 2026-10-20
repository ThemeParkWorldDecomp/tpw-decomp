#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "Ptr32.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

// 0x1004aa70 (200 bytes). Clears the drawn bit of each car in use and disables the draw of its ride's mesh (the animated kind,
// flag 0x4000, with the two HFieldXZ), then stops the train's sounds and clears the train's active bit (2).
void DeactivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    sCarInstance* car = train->mCars + static_cast<int32_t>(train->mF08);
    for (uint32_t count = train->mCarsInUse; count != 0; count--) {
        car->mFlags &= ~1u;
        if (car->mRide != 0) {
            tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
            if ((set->mFlags & 0x4000) != 0) {
                DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            } else {
                DisableMeshDraw(set->mHeads);
            }
        }
        car++;
    }
    StopTrainSounds(instance, train);
    train->mFlags &= ~2u;
}

// 0x1004a7d0 (116 bytes). The train's front sample: position plus the first car's sample over the coaster's scale plus its type's mF24
// over the scale; the rear: position plus the last car's sample over the scale minus its type's mF28 over the scale.
void GetTrainFrontAndRearSamples(tag_sCoasterInstance* instance, sTrainInstance* train) {
    sCarInstance* first = train->mCars + static_cast<int32_t>(train->mF08);
    sCarInstance* last = first + static_cast<int32_t>(train->mCarsInUse - 1);
    float front = train->mPosition + first->mF38 / instance->mFAC;
    front = front + first->mType->mF24 / instance->mFAC;
    train->mFrontSample = front;
    float rear = train->mPosition + last->mF38 / instance->mFAC;
    rear = rear - last->mType->mF28 / instance->mFAC;
    train->mRearSample = rear;
}

// 0x1004aedc (132 bytes). The speed a car wants at a height: a plus (the type's +0x28 going up, +0x2c going down, times the type's
// +0x24 times (c - d)); at least b. A NaN gives b.
float GetCarWantedSpeedAtHeight(sCoasterType* type, float a, float b, float c, float d) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(type);
    float f24, f28, f2c;
    std::memcpy(&f24, bytes + 0x24, 4);
    std::memcpy(&f28, bytes + 0x28, 4);
    std::memcpy(&f2c, bytes + 0x2c, 4);
    float slope = (d > c) ? f28 : f2c;
    float diff = c - d;
    float scaled = f24 * diff;
    float wanted = ppc_fmadds(slope, scaled, a);
    return !(wanted < b) ? wanted : b;   // bge is "not less": a NaN keeps wanted
}
