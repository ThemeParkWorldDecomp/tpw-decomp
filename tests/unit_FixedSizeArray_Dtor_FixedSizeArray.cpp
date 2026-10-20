// Test library for FixedSizeArray<BehaviourEntry>::~FixedSizeArray (unit v5733; libtpw_unit_FixedSizeArray_Dtor_FixedSizeArray.dylib,
// tests/test_FixedSizeArray_Dtor_FixedSizeArray.py), main app:
//   src/VehicleCallbacks_nav_FixedSizeArray_Dtor_FixedSizeArray.cpp
//
// The two runtime imports (__destroy_new_array, operator delete) are recording fakes, the same events as the PowerPC stubs.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "VehicleSteer.h"

using namespace calllog;

void FixedSizeArrayDestroyNewArray(void* array, int32_t destructor) {
    ev("DestroyArray", {lab(array), destructor});
}

void FixedSizeArrayOperatorDelete(void* object) {
    ev("Delete", {lab(object)});
}

extern "C" {

uint64_t tpw_fsa_addr(const char* name) {
    (void)name;
    return 0;
}

uint64_t tpw_fsa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fsa_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(FixedSizeArray<BehaviourEntry>, mData));
    return n;
}

uint64_t tpw_fsa_dtor(uint64_t self, int32_t flag) {
    FixedSizeArray<BehaviourEntry>* result =
        FixedSizeArray_BehaviourEntry_Dtor(reinterpret_cast<FixedSizeArray<BehaviourEntry>*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(result));
}

}  // extern "C"
