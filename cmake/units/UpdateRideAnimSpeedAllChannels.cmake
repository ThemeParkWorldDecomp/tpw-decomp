# Unit v51077: the ride animation leaves, the fly accessors, the ride pointers, Debug.cpp leaves, keyboard actions and static initialisers
# (src/game/Debug.cpp, src/game/keyboard.cpp, src/game/online/lobby_globe.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_screen.cpp, src/game/park/GoldenTickets.cpp, src/game/rides/FlyingRide.cpp, src/game/rides/RideAnim.cpp, src/game/rides/TPRide.cpp, src/game/save/BlockSave.cpp). The callees outside the unit are recording fakes in
# tests/unit_UpdateRideAnimSpeedAllChannels.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateRideAnimSpeedAllChannels src/game/Debug.cpp src/game/keyboard.cpp src/game/online/lobby_globe.cpp
    src/game/online/lobby_island.cpp src/game/online/lobby_screen.cpp src/game/park/GoldenTickets.cpp
    src/game/rides/FlyingRide.cpp src/game/rides/RideAnim.cpp src/game/rides/TPRide.cpp src/game/save/BlockSave.cpp
    platform/arena.cpp)
set(TPW_UNIT_UpdateRideAnimSpeedAllChannels_KEEP
  __Z13DEBUG_MESSAGEPKcj   # DEBUG_MESSAGE(char const*, unsigned int)
  __Z13FLY_SetRunwayii   # FLY_SetRunway(int, int)
  __Z13speed_up_timev   # speed_up_time()
  __Z14GetCurrentAnimP16tag_sRideAnimSetPjS1_j   # GetCurrentAnim(tag_sRideAnimSet*, unsigned int*, unsigned int*, unsigned int)
  __Z14slow_down_timev   # slow_down_time()
  __Z15GetRideAnimTimei   # GetRideAnimTime(int)
  __Z15SetRideAnimTimej   # SetRideAnimTime(unsigned int)
  __Z16FLY_GetCarInBasei   # FLY_GetCarInBase(int)
  __Z17FlushDeferredAnimP16tag_sRideAnimSetj   # FlushDeferredAnim(tag_sRideAnimSet*, unsigned int)
  __Z17__sinit_Debug_cppv   # __sinit_Debug_cpp()
  __Z18__sinit_TPRide_cppv   # __sinit_TPRide_cpp()
  __Z18abort_coaster_editv   # abort_coaster_edit()
  __Z21__sinit_BlockSave_cppv   # __sinit_BlockSave_cpp()
  __Z23__sinit_lobby_globe_cppv   # __sinit_lobby_globe_cpp()
  __Z24__sinit_lobby_island_cppv   # __sinit_lobby_island_cpp()
  __Z24__sinit_lobby_screen_cppv   # __sinit_lobby_screen_cpp()
  __Z25__sinit_GoldenTickets_cppv   # __sinit_GoldenTickets_cpp()
  __Z28GetAnimRemainingMillisecondsP16sAnimTimeControl   # GetAnimRemainingMilliseconds(sAnimTimeControl*)
  __Z28UpdateRideAnimSpeedImmediateP16tag_sRideAnimSetfi   # UpdateRideAnimSpeedImmediate(tag_sRideAnimSet*, float, int)
  __Z30UpdateRideAnimSpeedAllChannelsP16tag_sRideAnimSetf   # UpdateRideAnimSpeedAllChannels(tag_sRideAnimSet*, float)
  __Z39IslandOnlineParkInfoPanelCancelCallbackv   # IslandOnlineParkInfoPanelCancelCallback()
  _gV51077Debug5944
  _gV51077Debug5948
  _gV51077GlobeNode48ec
  _gV51077GlobeW48d8
  _gV51077IslandN48f8
  _gV51077IslandNode490c
  _gV51077IslandNode492c
  _gV51077IslandNode494c
  _gV51077IslandW4918
  _gV51077IslandW4938
  _gV51077RideNode1ef0
  _gV51077RideW1edc
  _gV51077ScreenNode49d0
  _gV51077ScreenW49bc
  _gV51077TicketNodec1f8
  _gV51077TicketWc1e4
)
