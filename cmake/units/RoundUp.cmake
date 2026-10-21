# Unit v5275: RoundUp and GetCoasterTrackStats (src/game/rides/CoasterSpline.cpp, src/game/rides/CoasterTrain.cpp). GetCoasterInstanceFromCObject is a recording fake in
# tests/unit_RoundUp.cpp; modf is the C library's.
set(TPW_UNIT_RoundUp src/game/rides/CoasterSpline.cpp src/game/rides/CoasterTrain.cpp platform/arena.cpp)
set(TPW_UNIT_RoundUp_KEEP
  __Z20GetCoasterTrackStatsPvPfS0_S0_Pj   # GetCoasterTrackStats(void*, float*, float*, float*, unsigned int*)
  __Z7RoundUpf   # RoundUp(float)
)
