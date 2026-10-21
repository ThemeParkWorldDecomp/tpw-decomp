# Unit v5252: eight advisor scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are recording fakes in
# tests/unit_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS_KEEP
  __ZN15CAdvisorScoring18_ADVISOR_TAKE_LOANEv   # CAdvisorScoring::_ADVISOR_TAKE_LOAN()
  __ZN15CAdvisorScoring20_ADVISOR_WEALTH_HIGHEv   # CAdvisorScoring::_ADVISOR_WEALTH_HIGH()
  __ZN15CAdvisorScoring22_ADVISOR_LOTS_HANDYMANEv   # CAdvisorScoring::_ADVISOR_LOTS_HANDYMAN()
  __ZN15CAdvisorScoring26_ADVISOR_LOTS_ENTERTAINERSEv   # CAdvisorScoring::_ADVISOR_LOTS_ENTERTAINERS()
  __ZN15CAdvisorScoring29_ADVISOR_GENERAL_LAX_SECURITYEv   # CAdvisorScoring::_ADVISOR_GENERAL_LAX_SECURITY()
  __ZN15CAdvisorScoring30_ADVISOR_IN_THE_RED_MONTH_LEFTEv   # CAdvisorScoring::_ADVISOR_IN_THE_RED_MONTH_LEFT()
  __ZN15CAdvisorScoring30_ADVISOR_IN_THE_RED_SIX_MONTHSEv   # CAdvisorScoring::_ADVISOR_IN_THE_RED_SIX_MONTHS()
  __ZN15CAdvisorScoring32_ADVISOR_IN_THE_RED_THREE_MONTHSEv   # CAdvisorScoring::_ADVISOR_IN_THE_RED_THREE_MONTHS()
)
