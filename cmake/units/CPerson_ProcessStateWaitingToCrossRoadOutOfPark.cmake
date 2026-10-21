# Unit v5006: the three states of a guest leaving the park (src/game/people/PersonQueue.cpp). The callees outside the unit are
# recording fakes in tests/unit_CPerson_ProcessStateWaitingToCrossRoadOutOfPark.cpp, which also defines gWorld and gWorldMap.
set(TPW_UNIT_CPerson_ProcessStateWaitingToCrossRoadOutOfPark src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_ProcessStateWaitingToCrossRoadOutOfPark_KEEP
  __ZN7CPerson23ProcessStateLeavingParkEv   # CPerson::ProcessStateLeavingPark()
  __ZN7CPerson34ProcessStateWaitingForBusOuttaHereEv   # CPerson::ProcessStateWaitingForBusOuttaHere()
  __ZN7CPerson39ProcessStateWaitingToCrossRoadOutOfParkEv   # CPerson::ProcessStateWaitingToCrossRoadOutOfPark()
)
