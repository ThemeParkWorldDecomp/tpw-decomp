# The lobby's small leaves part 8 (unit w88): CRideDatabase::DeleteCustomEffects, PauseUI, TogglePauseUI, ShowPauseWindow, TogglePauseWindow and
# PauseWindowMsg (with the pause window's script and animation, and the real gPauseWindow of ui_mainpanel2_pause.cpp). The emitter and sound
# calls, the timers, Dialog, the default dialog callback, the window's GetPosition, setPivot and startAnim, the key table's Search and the
# game's TogglePause are fakes in the unit file; the rectangle the message function builds is the real CoolRect (src/UI_Base.cpp).
set(TPW_UNIT_LobbyLeaves8 src/RideDB_effects.cpp src/ui_main_pause.cpp src/ui_mainpanel2_window.cpp src/ui_pause_data.cpp
    src/ui_mainpanel2_pause.cpp src/UI_Base.cpp platform/arena.cpp)
