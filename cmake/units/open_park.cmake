# Unit v51108: the keyboard cheat actions (src/game/keyboard.cpp). The callees outside the unit are
# recording fakes in tests/unit_open_park.cpp, which also defines the globals of other files.
set(TPW_UNIT_open_park src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_open_park_KEEP
  __Z10close_parkv   # close_park()
  __Z11spawn_guardv   # spawn_guard()
  __Z12WeatherResetv   # WeatherReset()
  __Z13get_doshed_upv   # get_doshed_up()
  __Z13populate_parkv   # populate_park()
  __Z14spawn_mechanicv   # spawn_mechanic()
  __Z16FPSDisplayTogglev   # FPSDisplayToggle()
  __Z16Screen_BuyObjectv   # Screen_BuyObject()
  __Z16spawn_researcherv   # spawn_researcher()
  __Z16togglegamewindowv   # togglegamewindow()
  __Z17normal_speed_timev   # normal_speed_time()
  __Z19continuous_snapshotv   # continuous_snapshot()
  __Z21complete_all_researchv   # complete_all_research()
  __Z22create_arrival_vehiclev   # create_arrival_vehicle()
  __Z23uncomplete_all_researchv   # uncomplete_all_research()
  __Z25start_bankruptcy_sequencev   # start_bankruptcy_sequence()
  __Z8mainmenuv   # mainmenu()
  __Z8snapshotv   # snapshot()
  __Z9open_parkv   # open_park()
  __Z9togglehudv   # togglehud()
  __ZN16keyaction_callee11OperatorNewEj   # keyaction_callee::OperatorNew(unsigned int)
  _gKeyHudToggle
  _gKeyMovieToggle
)
