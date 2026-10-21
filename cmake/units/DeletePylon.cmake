# Unit v5893: RemoveTrackBraceFromPylon, DeletePylon, MoveBrace (src/game/rides/CoasterEdit.cpp). The callees outside
# the unit are recording fakes in tests/unit_DeletePylon.cpp, which also defines the globals of other files.
set(TPW_UNIT_DeletePylon src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_DeletePylon_KEEP
  __Z11DeletePylonP20tag_sCoasterInstanceP15sTrackBraceDatai   # DeletePylon(tag_sCoasterInstance*, sTrackBraceData*, int)
  __Z25RemoveTrackBraceFromPylonP20tag_sCoasterInstanceP15tag_sTrackBrace   # RemoveTrackBraceFromPylon(tag_sCoasterInstance*, tag_sTrackBrace*)
  __Z9MoveBraceP20tag_sCoasterInstanceP15tag_sTrackBraceP15sTrackBraceDatajjPfS5_ii   # MoveBrace(tag_sCoasterInstance*, tag_sTrackBrace*, sTrackBraceData*, unsigned int, unsigned int, float*, float*, int, int)
)
