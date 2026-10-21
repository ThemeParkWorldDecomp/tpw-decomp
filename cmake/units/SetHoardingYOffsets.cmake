# Unit v5674: SetHoardingYOffsets and GetRotatedHFieldCoord (src/game/park/TPHoardings.cpp). FloatToCell is a recording fake in
# tests/unit_SetHoardingYOffsets.cpp, which also defines gHeightfieldHeader.
set(TPW_UNIT_SetHoardingYOffsets src/game/park/TPHoardings.cpp platform/arena.cpp)
set(TPW_UNIT_SetHoardingYOffsets_KEEP
  __Z19SetHoardingYOffsetsP16tag_sRideAnimSet   # SetHoardingYOffsets(tag_sRideAnimSet*)
  __Z21GetRotatedHFieldCoordiffffffPjS_   # GetRotatedHFieldCoord(int, float, float, float, float, float, float, unsigned int*, unsigned int*)
)
