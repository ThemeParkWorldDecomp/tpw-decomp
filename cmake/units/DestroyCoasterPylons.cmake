# Unit v5038: the coaster track and pylon destroyers and the track position control (src/game/rides/CoasterTrack.cpp). The callees outside the unit are recording fakes in
# tests/unit_DestroyCoasterPylons.cpp.
set(TPW_UNIT_DestroyCoasterPylons src/game/rides/CoasterTrack.cpp platform/arena.cpp)
set(TPW_UNIT_DestroyCoasterPylons_KEEP
  __Z16GetTrackPositionP17sTrackPosnControlfP8s3DPoint   # GetTrackPosition(sTrackPosnControl*, float, s3DPoint*)
  __Z19DestroyCoasterTrackP20tag_sCoasterInstance   # DestroyCoasterTrack(tag_sCoasterInstance*)
  __Z20DestroyCoasterPylonsP20tag_sCoasterInstance   # DestroyCoasterPylons(tag_sCoasterInstance*)
  __Z25DecrementTrackPosnControlP17sTrackPosnControl   # DecrementTrackPosnControl(sTrackPosnControl*)
  __Z25IncrementTrackPosnControlP17sTrackPosnControl   # IncrementTrackPosnControl(sTrackPosnControl*)
)
