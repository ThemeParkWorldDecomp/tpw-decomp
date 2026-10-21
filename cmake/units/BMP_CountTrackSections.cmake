# Unit v5344: BMP_CountTrackSections, BMP_FreeTrack, BMP_SetRidePerformance, BMP_DestroyRide (src/game/rides/BumperRide.cpp).
# The callees are recording fakes in tests/unit_BMP_CountTrackSections.cpp, which also defines gColList.
set(TPW_UNIT_BMP_CountTrackSections src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_CountTrackSections_KEEP
  __Z13BMP_FreeTracki   # BMP_FreeTrack(int)
  __Z15BMP_DestroyRidei   # BMP_DestroyRide(int)
  __Z22BMP_CountTrackSectionsiPiS_S_S_   # BMP_CountTrackSections(int, int*, int*, int*, int*)
  __Z22BMP_SetRidePerformanceii   # BMP_SetRidePerformance(int, int)
  _gRidePerfMax
  _gRidePerfMin
)
