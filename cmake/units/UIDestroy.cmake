# The window's destructor chain (unit w51): ~InterfaceWindow, PropagateDestruction, UnCaptureKeys, UI_RemoveTimer, FindTimer, ClearAllPointers, the
# three UI_Invalidate*Messages and the two queues' InvalidateMsgs. The window's constructor, UI_SendMessage and the rectangles are real; the
# draw objects, the regions, the help text, the pointer update and the message poster are fakes in the unit file.
# Unit w54 adds the leftovers on the destroy path: the base window's small virtuals (InterfaceWindow_virtuals.cpp), the bar's and the text box's
# destructors (ui_controls_dtors.cpp), the timer's destructor (UI_timers_remove.cpp), the string heap's ReleaseString (lobby_start_leaves.cpp)
# and the button (InterfaceButton.cpp, for its overrides of Enable and GetHelpText and its destructor).
set(TPW_UNIT_UIDestroy src/InterfaceWindow_dtor.cpp src/UI_timers_remove.cpp src/UI_invalidate.cpp src/UI_send.cpp src/InterfaceWindow.cpp
    src/UI_Base.cpp src/InterfaceWindow_virtuals.cpp src/ui_controls_dtors.cpp src/lobby_start_leaves.cpp src/InterfaceButton.cpp)
