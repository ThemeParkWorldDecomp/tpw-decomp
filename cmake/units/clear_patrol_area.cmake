# Unit v5341: start_strike, clear_patrol_area, wheres_my_script, abort_all_requesters (src/game/keyboard.cpp).
# The callees outside the unit are recording fakes in tests/unit_clear_patrol_area.cpp, which also defines the globals of other files.
set(TPW_UNIT_clear_patrol_area src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_clear_patrol_area_KEEP
  __Z12start_strikev   # start_strike()
  __Z16wheres_my_scriptv   # wheres_my_script()
  __Z17clear_patrol_areav   # clear_patrol_area()
  __Z20abort_all_requestersv   # abort_all_requesters()
)
