# Unit v5365: DestroyTrackBrace and PlaceEntrancePylons (src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_DestroyTrackBrace.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_DestroyTrackBrace src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_DestroyTrackBrace_KEEP
  __Z17DestroyTrackBraceP15tag_sTrackBraceP13sPylonControl   # DestroyTrackBrace(tag_sTrackBrace*, sPylonControl*)
  __Z19PlaceEntrancePylonsP20tag_sCoasterInstancejjfi   # PlaceEntrancePylons(tag_sCoasterInstance*, unsigned int, unsigned int, float, int)
)
