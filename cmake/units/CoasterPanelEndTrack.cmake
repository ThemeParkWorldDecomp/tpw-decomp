# Unit v5333: CoasterPanelEndTrack (src/game/ui/ui_coasterPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CoasterPanelEndTrack.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterPanelEndTrack src/game/ui/ui_coasterPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterPanelEndTrack_KEEP
  __Z20CoasterPanelEndTrackv   # CoasterPanelEndTrack()
  __ZN18coaster_end_callee11OperatorNewEj   # coaster_end_callee::OperatorNew(unsigned int)
)
