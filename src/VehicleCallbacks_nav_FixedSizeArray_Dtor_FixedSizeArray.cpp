#include "VehicleSteer.h"
#include <cstddef>
#include <cstdlib>
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "TPCamera.h"
#include "vector2d.h"
#include "World.h"

// Weak defaults for the two runtime imports; the launcher's runtime (or a test library) may define them.
__attribute__((weak)) void FixedSizeArrayDestroyNewArray(void* array, int32_t destructor) {
    (void)array;
    (void)destructor;
}
__attribute__((weak)) void FixedSizeArrayOperatorDelete(void* object) {
    (void)object;
}

// 0x101355dc (96 bytes): FixedSizeArray<BehaviourEntry>::~FixedSizeArray
FixedSizeArray<BehaviourEntry>* FixedSizeArray_BehaviourEntry_Dtor(FixedSizeArray<BehaviourEntry>* self, int16_t deleteFlag) {
    if (self != nullptr) {
        FixedSizeArrayDestroyNewArray(self->mData, 0);
        if (deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    }
    return self;
}
