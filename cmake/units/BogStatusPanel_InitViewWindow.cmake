# Unit v5301: the bog status panel's window functions (src/game/ui/ui_bogStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_BogStatusPanel_InitViewWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_BogStatusPanel_InitViewWindow src/game/ui/ui_bogStatus.cpp platform/arena.cpp)
set(TPW_UNIT_BogStatusPanel_InitViewWindow_KEEP
  __ZN14BogStatusPanel10InitWindowEv   # BogStatusPanel::InitWindow()
  __ZN14BogStatusPanel12UpdateWindowEv   # BogStatusPanel::UpdateWindow()
  __ZN14BogStatusPanel14InitViewWindowEv   # BogStatusPanel::InitViewWindow()
  __ZN14BogStatusPanel16UpdateViewWindowEv   # BogStatusPanel::UpdateViewWindow()
  __ZN14BogStatusPanel4ShowEP6CThing   # BogStatusPanel::Show(CThing*)
  _gBogStatusScript
  _gBogStatusThing
)
