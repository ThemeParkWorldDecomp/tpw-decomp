# Unit v5213: the ride preview (CreateRidePreview, DetermineRidePreviewScale; src/game/rides/RidePreview.cpp). The callees outside the unit are recording
# fakes in tests/unit_CreateRidePreview.cpp, which also defines gRideAnimSets and gRideTypes.
set(TPW_UNIT_CreateRidePreview src/game/rides/RidePreview.cpp platform/arena.cpp)
set(TPW_UNIT_CreateRidePreview_KEEP
  __Z17CreateRidePreviewfffffj   # CreateRidePreview(float, float, float, float, float, unsigned int)
  __Z25DetermineRidePreviewScalejffPfS_S_S_S_S_S_S_S_   # DetermineRidePreviewScale(unsigned int, float, float, float*, float*, float*, float*, float*, float*, float*, float*, float*)
  _gRidePreviewBlock
)
