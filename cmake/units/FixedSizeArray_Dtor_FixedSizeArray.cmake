# Unit v5733: FixedSizeArray<BehaviourEntry>::~FixedSizeArray (src/game/rides/VehicleCallbacks.cpp). The runtime imports are recording fakes in
# tests/unit_FixedSizeArray_Dtor_FixedSizeArray.cpp.
set(TPW_UNIT_FixedSizeArray_Dtor_FixedSizeArray src/game/rides/VehicleCallbacks.cpp platform/arena.cpp)
set(TPW_UNIT_FixedSizeArray_Dtor_FixedSizeArray_KEEP
  __Z28FixedSizeArrayOperatorDeletePv   # FixedSizeArrayOperatorDelete(void*)
  __Z29FixedSizeArrayDestroyNewArrayPvi   # FixedSizeArrayDestroyNewArray(void*, int)
  __Z34FixedSizeArray_BehaviourEntry_DtorP14FixedSizeArrayI14BehaviourEntryEs   # FixedSizeArray_BehaviourEntry_Dtor(FixedSizeArray<BehaviourEntry>*, short)
)
