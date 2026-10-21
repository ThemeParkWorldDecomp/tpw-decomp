# Unit v5664: LinearSampleTrack (src/game/rides/CoasterTrack.cpp). The callees outside the unit are recording
# fakes in tests/unit_LinearSampleTrack.cpp, which also defines gCoasterCurrentData.
set(TPW_UNIT_LinearSampleTrack src/game/rides/CoasterTrack.cpp platform/arena.cpp)
set(TPW_UNIT_LinearSampleTrack_KEEP
  __Z17LinearSampleTrackP20tag_sCoasterInstance   # LinearSampleTrack(tag_sCoasterInstance*)
)
