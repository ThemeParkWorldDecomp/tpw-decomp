# Unit v5971: the open / close park, researcher and coverage scorers and GetHighestScoringFromNext (src/game/advisor/AdvisorScoring.cpp).
# The callees outside the unit are recording fakes in tests/unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.cpp, which also
# defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_ src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS__KEEP
  __ZN15CAdvisorScoring18_ADVISOR_OPEN_PARKEv   # CAdvisorScoring::_ADVISOR_OPEN_PARK()
  __ZN15CAdvisorScoring19_ADVISOR_CLOSE_PARKEv   # CAdvisorScoring::_ADVISOR_CLOSE_PARK()
  __ZN15CAdvisorScoring25GetHighestScoringFromNextEi   # CAdvisorScoring::GetHighestScoringFromNext(int)
  __ZN15CAdvisorScoring32_ADVISOR_STAFF_HANDYMAN_COVERAGEEv   # CAdvisorScoring::_ADVISOR_STAFF_HANDYMAN_COVERAGE()
  __ZN15CAdvisorScoring32_ADVISOR_STAFF_MECHANIC_COVERAGEEv   # CAdvisorScoring::_ADVISOR_STAFF_MECHANIC_COVERAGE()
  __ZN15CAdvisorScoring32_ADVISOR_STAFF_TRAIN_RESEARCHERSEv   # CAdvisorScoring::_ADVISOR_STAFF_TRAIN_RESEARCHERS()
  __ZN15CAdvisorScoring33_ADVISOR_STAFF_HIRE_RESEARCHERS_1Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_1()
  __ZN15CAdvisorScoring33_ADVISOR_STAFF_HIRE_RESEARCHERS_3Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_3()
)
