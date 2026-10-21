# Unit v5679: CAdvisor::HandleEvent (src/game/advisor/Advisor.cpp). The callees outside the unit are recording fakes in
# tests/unit_CAdvisor_HandleEvent.cpp, which also defines TheScoring and the other globals of other files.
set(TPW_UNIT_CAdvisor_HandleEvent src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisor_HandleEvent_KEEP
  __ZN8CAdvisor11HandleEventE9EventType   # CAdvisor::HandleEvent(EventType)
)
