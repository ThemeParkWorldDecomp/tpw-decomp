# Unit v5933: OutputAnimTimeStats and RestoreRideInstance (src/game/rides/TPRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_OutputAnimTimeStats.cpp, which also defines the globals of other files.
set(TPW_UNIT_OutputAnimTimeStats src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_OutputAnimTimeStats_KEEP
  __Z19OutputAnimTimeStatsP16sAnimTimeControlP16tag_sRideAnimSet   # OutputAnimTimeStats(sAnimTimeControl*, tag_sRideAnimSet*)
  __Z19RestoreRideInstancejjjP16sFixedRideParamsjPviii   # RestoreRideInstance(unsigned int, unsigned int, unsigned int, sFixedRideParams*, unsigned int, void*, int, int, int)
)
