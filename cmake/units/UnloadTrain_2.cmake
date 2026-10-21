# Unit v5900: coaster train loading (UnloadTrain, KickPeepsFromEntranceQueue, LoadTrain, CoasterSetCapacity, SetEntranceLinearSamples,
# GetCarsInStation, HandleTrainLoading; src/game/rides/CoasterQueue.cpp). The callees outside the unit are recording fakes in
# tests/unit_UnloadTrain_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_UnloadTrain_2 src/game/rides/CoasterQueue.cpp platform/arena.cpp)
set(TPW_UNIT_UnloadTrain_2_KEEP
  __Z11UnloadTrainP20tag_sCoasterInstanceP14sTrainInstancei   # UnloadTrain(tag_sCoasterInstance*, sTrainInstance*, int)
  __Z16GetCarsInStationP20tag_sCoasterInstanceP14sTrainInstancePjS3_   # GetCarsInStation(tag_sCoasterInstance*, sTrainInstance*, unsigned int*, unsigned int*)
  __Z18CoasterSetCapacityji   # CoasterSetCapacity(unsigned int, int)
  __Z18HandleTrainLoadingP20tag_sCoasterInstanceP14sTrainInstance   # HandleTrainLoading(tag_sCoasterInstance*, sTrainInstance*)
  __Z24SetEntranceLinearSamplesP20tag_sCoasterInstance   # SetEntranceLinearSamples(tag_sCoasterInstance*)
  __Z26KickPeepsFromEntranceQueueP20tag_sCoasterInstance   # KickPeepsFromEntranceQueue(tag_sCoasterInstance*)
  __Z9LoadTrainP20tag_sCoasterInstanceP14sTrainInstancejj   # LoadTrain(tag_sCoasterInstance*, sTrainInstance*, unsigned int, unsigned int)
  _gCoasterCapacityFloor
  _gKickPeepsFloor
)
