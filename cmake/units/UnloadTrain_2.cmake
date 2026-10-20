# Unit v5900: coaster train loading (UnloadTrain, KickPeepsFromEntranceQueue, LoadTrain, CoasterSetCapacity, SetEntranceLinearSamples,
# GetCarsInStation, HandleTrainLoading; src/AllocateQueue_UnloadTrain_2.cpp). The callees outside the unit are recording fakes in
# tests/unit_UnloadTrain_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_UnloadTrain_2 src/AllocateQueue_UnloadTrain_2.cpp platform/arena.cpp)
