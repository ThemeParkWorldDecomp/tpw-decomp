# Unit v5545: the backtrack key (src/game/keyboard.cpp). The callees outside the unit are recording
# fakes in tests/unit_backtrack.cpp, which also defines the globals of other files.
set(TPW_UNIT_backtrack src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_backtrack_KEEP
  __Z9backtrackv   # backtrack()
)
