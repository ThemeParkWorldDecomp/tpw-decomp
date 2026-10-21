# Unit v5337: DeleteTrackBrace and DeleteLastTrackBrace (src/game/rides/CoasterEdit.cpp). The callees outside the unit
# are recording fakes in tests/unit_DeleteLastTrackBrace.cpp, which also defines the globals of other files.
set(TPW_UNIT_DeleteLastTrackBrace src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_DeleteLastTrackBrace_KEEP
  __Z16DeleteTrackBraceP20tag_sCoasterInstanceP15tag_sTrackBrace   # DeleteTrackBrace(tag_sCoasterInstance*, tag_sTrackBrace*)
  __Z20DeleteLastTrackBracev   # DeleteLastTrackBrace()
)
