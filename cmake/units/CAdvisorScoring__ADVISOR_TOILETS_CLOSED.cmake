# Unit v5040: eight advisor scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are recording fakes in
# tests/unit_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TOILETS_CLOSED src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TOILETS_CLOSED_KEEP
  __ZN15CAdvisorScoring21_ADVISOR_RIDES_CLOSEDEv   # CAdvisorScoring::_ADVISOR_RIDES_CLOSED()
  __ZN15CAdvisorScoring21_ADVISOR_SHOPS_CLOSEDEv   # CAdvisorScoring::_ADVISOR_SHOPS_CLOSED()
  __ZN15CAdvisorScoring23_ADVISOR_LOTS_OF_PRANKSEv   # CAdvisorScoring::_ADVISOR_LOTS_OF_PRANKS()
  __ZN15CAdvisorScoring23_ADVISOR_TOILETS_CLOSEDEv   # CAdvisorScoring::_ADVISOR_TOILETS_CLOSED()
  __ZN15CAdvisorScoring25_ADVISOR_SIDESHOWS_CLOSEDEv   # CAdvisorScoring::_ADVISOR_SIDESHOWS_CLOSED()
  __ZN15CAdvisorScoring25_ADVISOR_STAFFROOM_CLOSEDEv   # CAdvisorScoring::_ADVISOR_STAFFROOM_CLOSED()
  __ZN15CAdvisorScoring38_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONEEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE()
  __ZN15CAdvisorScoring53_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTIONEv   # CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION()
  _gSideshowNoneId
)
