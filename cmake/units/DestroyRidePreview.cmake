# Unit v5977: DestroyRidePreview, HighlightRide, TickRidePreview (src/game/rides/RidePreview.cpp). The callees outside the unit are
# recording fakes in tests/unit_DestroyRidePreview.cpp, which also defines the globals of other files.
set(TPW_UNIT_DestroyRidePreview src/game/rides/RidePreview.cpp platform/arena.cpp)
set(TPW_UNIT_DestroyRidePreview_KEEP
  __Z13HighlightRidev   # HighlightRide()
  __Z15TickRidePreviewP16sRidePreviewInfo   # TickRidePreview(sRidePreviewInfo*)
  __Z18DestroyRidePreviewP16sRidePreviewInfo   # DestroyRidePreview(sRidePreviewInfo*)
)
