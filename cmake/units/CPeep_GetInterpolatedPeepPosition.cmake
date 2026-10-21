# Unit v5533: CPeep::GetInterpolatedPeepPosition and CPeep::Serialise (src/game/people/Peep.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPeep_GetInterpolatedPeepPosition.cpp.
set(TPW_UNIT_CPeep_GetInterpolatedPeepPosition src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_GetInterpolatedPeepPosition_KEEP
  __ZN5CPeep27GetInterpolatedPeepPositionEfiRfS0_S0_Rj   # CPeep::GetInterpolatedPeepPosition(float, int, float&, float&, float&, unsigned int&)
  __ZN5CPeep9SerialiseEPviij   # CPeep::Serialise(void*, int, int, unsigned int)
  ___register_global_object
)
