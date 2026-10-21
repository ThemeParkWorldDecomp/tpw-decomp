# Unit v5519: BogStatusPanel::InitStatusWindow and UpdateStatusWindow (src/game/ui/ui_bogStatus.cpp). The callees
# outside the unit are recording fakes in tests/unit_BogStatusPanel_UpdateStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_BogStatusPanel_UpdateStatusWindow src/game/ui/ui_bogStatus.cpp platform/arena.cpp)
set(TPW_UNIT_BogStatusPanel_UpdateStatusWindow_KEEP
  __ZN10bog_callee13HGradientCtorEPvPKc   # bog_callee::HGradientCtor(void*, char const*)
  __ZN14BogStatusPanel16InitStatusWindowEv   # BogStatusPanel::InitStatusWindow()
  __ZN14BogStatusPanel18UpdateStatusWindowEv   # BogStatusPanel::UpdateStatusWindow()
)
