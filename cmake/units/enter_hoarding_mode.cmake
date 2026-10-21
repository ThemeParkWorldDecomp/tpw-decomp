# Unit v5296: win_ticket_7/8/9, enter_hoarding_mode, force_ride_breakdown, park_attraction_value. The callees outside the unit are recording
# fakes in tests/unit_enter_hoarding_mode.cpp, which also defines the globals of other files.
set(TPW_UNIT_enter_hoarding_mode src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_enter_hoarding_mode_KEEP
  __Z12win_ticket_7v   # win_ticket_7()
  __Z12win_ticket_8v   # win_ticket_8()
  __Z12win_ticket_9v   # win_ticket_9()
  __Z19enter_hoarding_modev   # enter_hoarding_mode()
  __Z20force_ride_breakdownv   # force_ride_breakdown()
  __Z21park_attraction_valuev   # park_attraction_value()
)
