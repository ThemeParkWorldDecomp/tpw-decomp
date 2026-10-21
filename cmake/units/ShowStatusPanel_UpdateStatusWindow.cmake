# Unit v5547: ShowStatusPanel::UpdateStatusWindow, ApplyChanges, applytoallcallback, Show (src/game/ui/ui_showStatus.cpp). The callees
# outside the unit are recording fakes in tests/unit_ShowStatusPanel_UpdateStatusWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowStatusPanel_UpdateStatusWindow src/game/ui/ui_showStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShowStatusPanel_UpdateStatusWindow_KEEP
  __ZN15ShowStatusPanel12ApplyChangesEv   # ShowStatusPanel::ApplyChanges()
  __ZN15ShowStatusPanel18UpdateStatusWindowEv   # ShowStatusPanel::UpdateStatusWindow()
  __ZN15ShowStatusPanel18applytoallcallbackEP6CThing   # ShowStatusPanel::applytoallcallback(CThing*)
  __ZN15ShowStatusPanel4ShowEP6CThing   # ShowStatusPanel::Show(CThing*)
  _gShowStatusScript
  _gShowStatusThing
)
