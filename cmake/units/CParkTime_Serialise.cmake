# Unit v5222: CParkTime::Serialise (src/game/people/ParkTime.cpp). The six SAVEVAR/LOADVAR callees are recording fakes in
# tests/unit_CParkTime_Serialise.cpp.
set(TPW_UNIT_CParkTime_Serialise src/game/people/ParkTime.cpp platform/arena.cpp)
set(TPW_UNIT_CParkTime_Serialise_KEEP
  __ZN9CParkTime9SerialiseEPviij   # CParkTime::Serialise(void*, int, int, unsigned int)
)
