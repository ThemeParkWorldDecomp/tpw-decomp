# Unit v5369: RecalcDistancesFromCentreCar, SetTrainCapacity, ActivateCoasterTrain (src/game/rides/CoasterQueue.cpp, src/game/rides/CoasterTrain.cpp). The callees outside
# the unit are recording fakes in tests/unit_RecalcDistancesFromCentreCar.cpp, which also defines gRideAnimSets.
set(TPW_UNIT_RecalcDistancesFromCentreCar src/game/rides/CoasterQueue.cpp src/game/rides/CoasterTrain.cpp
    platform/arena.cpp)
set(TPW_UNIT_RecalcDistancesFromCentreCar_KEEP
  __Z16SetTrainCapacityP20tag_sCoasterInstancejjj   # SetTrainCapacity(tag_sCoasterInstance*, unsigned int, unsigned int, unsigned int)
  __Z20ActivateCoasterTrainP20tag_sCoasterInstanceP14sTrainInstance   # ActivateCoasterTrain(tag_sCoasterInstance*, sTrainInstance*)
  __Z28RecalcDistancesFromCentreCarP20tag_sCoasterInstanceP14sTrainInstance   # RecalcDistancesFromCentreCar(tag_sCoasterInstance*, sTrainInstance*)
)
