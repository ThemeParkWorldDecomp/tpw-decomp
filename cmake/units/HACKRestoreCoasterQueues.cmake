# Unit v51023: HACKRestoreCoasterQueues, NormaliseVectorXZ, DisableCoasterCarDrawSafe, IncCoasterConst1 (src/game/rides/CoasterSounds.cpp, src/game/rides/CoasterTrack.cpp, src/game/rides/CoasterTrain.cpp, src/game/rides/TPCoaster.cpp).
# The callees are recording fakes in tests/unit_HACKRestoreCoasterQueues.cpp, which also defines the globals of other files.
set(TPW_UNIT_HACKRestoreCoasterQueues src/game/rides/CoasterSounds.cpp src/game/rides/CoasterTrack.cpp
    src/game/rides/CoasterTrain.cpp src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_HACKRestoreCoasterQueues_KEEP
  __Z16IncCoasterConst1v   # IncCoasterConst1()
  __Z17NormaliseVectorXZR8s3DPoint   # NormaliseVectorXZ(s3DPoint&)
  __Z24HACKRestoreCoasterQueuesj   # HACKRestoreCoasterQueues(unsigned int)
  __Z25DisableCoasterCarDrawSafej   # DisableCoasterCarDrawSafe(unsigned int)
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
