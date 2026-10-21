# Unit v5576: GetAnimTimeStats, RestoreAnimTimeStats, GetNumActiveChannels (src/game/rides/RideAnim.cpp). No callees.
set(TPW_UNIT_GetAnimTimeStats src/game/rides/RideAnim.cpp platform/arena.cpp)
set(TPW_UNIT_GetAnimTimeStats_KEEP
  __Z16GetAnimTimeStatsP16sAnimTimeControlPj   # GetAnimTimeStats(sAnimTimeControl*, unsigned int*)
  __Z20GetNumActiveChannelsP16tag_sRideAnimSet   # GetNumActiveChannels(tag_sRideAnimSet*)
  __Z20RestoreAnimTimeStatsP16sAnimTimeControlP15tag_sAnimHeader   # RestoreAnimTimeStats(sAnimTimeControl*, tag_sAnimHeader*)
)
