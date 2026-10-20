# The lobby's small leaves part 5 (unit w64): HidePauseWindow, the string heap's DeAllocate, the end of the UI system (UI_CloseSystem,
# UI_CloseTimerSystem and the two queues' destructors). The timer's destructor is real (src/UI_timers_remove.cpp) and so are the rectangles the
# window's constructor needs (src/UI_Base.cpp); the window's members, the send and keyboard-capture calls, the draw object library and the
# cursor library are fakes in the unit file.
set(TPW_UNIT_LobbyLeaves5 src/ui_mainpanel2_pause.cpp src/UI_close.cpp src/lobby_start_leaves.cpp src/UI_timers_remove.cpp src/UI_Base.cpp)
