# Unit v5034: eight key actions (src/game/keyboard.cpp). The callees outside the unit are recording fakes in
# tests/unit_turn_object_counterclockwise.cpp, which also defines the globals of other files.
set(TPW_UNIT_turn_object_counterclockwise src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_turn_object_counterclockwise_KEEP
  __Z11TestAdvisorv   # TestAdvisor()
  __Z11place_vomitv   # place_vomit()
  __Z16quickload_layoutv   # quickload_layout()
  __Z16quicksave_layoutv   # quicksave_layout()
  __Z21turn_object_clockwisev   # turn_object_clockwise()
  __Z28turn_object_counterclockwisev   # turn_object_counterclockwise()
  __Z9quickloadv   # quickload()
  __Z9quicksavev   # quicksave()
  _gQuickSaveSuffix
  _gTestAdvisorIndex
)
