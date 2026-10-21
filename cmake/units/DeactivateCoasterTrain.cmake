# Unit v5763: DeactivateCoasterTrain, GetTrainFrontAndRearSamples, GetCarWantedSpeedAtHeight (src/game/rides/CoasterTrain.cpp).
# The callees outside the unit are recording fakes in tests/unit_DeactivateCoasterTrain.cpp, which also defines gRideAnimSets.
set(TPW_UNIT_DeactivateCoasterTrain src/game/rides/CoasterTrain.cpp platform/arena.cpp)
set(TPW_UNIT_DeactivateCoasterTrain_KEEP
  __Z22DeactivateCoasterTrainP20tag_sCoasterInstanceP14sTrainInstance   # DeactivateCoasterTrain(tag_sCoasterInstance*, sTrainInstance*)
  __Z25GetCarWantedSpeedAtHeightP12sCoasterTypeffff   # GetCarWantedSpeedAtHeight(sCoasterType*, float, float, float, float)
  __Z27GetTrainFrontAndRearSamplesP20tag_sCoasterInstanceP14sTrainInstance   # GetTrainFrontAndRearSamples(tag_sCoasterInstance*, sTrainInstance*)
)
