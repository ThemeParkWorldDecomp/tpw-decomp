# Unit v5184: the research panel's SpeedTimerFudge, PercentComplete and ResearchSpeed (src/game/ui/ui_researchPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_SpeedTimerFudge.cpp, which also defines the globals of other files.
set(TPW_UNIT_SpeedTimerFudge src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_SpeedTimerFudge_KEEP
  __Z13ResearchSpeedi   # ResearchSpeed(int)
  __Z15PercentCompletei   # PercentComplete(int)
  __Z15SpeedTimerFudgeP15InterfaceWindowi   # SpeedTimerFudge(InterfaceWindow*, int)
  _gResearchProjectNone
)
