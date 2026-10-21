# Unit v5561: RestoreAnimTime (src/game/rides/TPRide.cpp). IsValidAnimation and RestoreAnimTimeStats are recording fakes in
# tests/unit_RestoreAnimTime.cpp.
set(TPW_UNIT_RestoreAnimTime src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_RestoreAnimTime_KEEP
  __Z15RestoreAnimTimeP16tag_sRideAnimSetP16sAnimTimeControlP20sAnimTimeControlSave   # RestoreAnimTime(tag_sRideAnimSet*, sAnimTimeControl*, sAnimTimeControlSave*)
)
