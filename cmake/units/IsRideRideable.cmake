# Unit v5764: StopRidingRideIn1stPerson, GetThingAtPosnIfEntrance, IsRideRideable (src/game/TPCamera.cpp). The callees outside the
# unit are recording fakes in tests/unit_IsRideRideable.cpp, which also defines the globals of other files.
set(TPW_UNIT_IsRideRideable src/game/TPCamera.cpp platform/arena.cpp)
set(TPW_UNIT_IsRideRideable_KEEP
  __Z14IsRideRideablePvj   # IsRideRideable(void*, unsigned int)
  __Z24GetThingAtPosnIfEntranceff   # GetThingAtPosnIfEntrance(float, float)
  __Z25StopRidingRideIn1stPersonv   # StopRidingRideIn1stPerson()
)
