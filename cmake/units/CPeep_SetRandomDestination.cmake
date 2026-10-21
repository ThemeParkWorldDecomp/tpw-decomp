# Unit v5989: CPeep::SetRandomDestination (src/game/people/Peep.cpp). The callees
# outside the unit are recording fakes in tests/unit_CPeep_SetRandomDestination.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPeep_SetRandomDestination src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_SetRandomDestination_KEEP
  __ZN5CPeep20SetRandomDestinationEv   # CPeep::SetRandomDestination()
  _gPeepNoCell
)
