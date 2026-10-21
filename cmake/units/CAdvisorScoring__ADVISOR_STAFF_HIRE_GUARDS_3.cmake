# Unit v5265: seven staff scorers of CAdvisorScoring (src/game/advisor/AdvisorScoring.cpp). The callees outside
# the unit are recording fakes in tests/unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.cpp, which also defines gGameWorld.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3 src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3_KEEP
  __ZN15CAdvisorScoring28_ADVISOR_STAFF_HIRE_GUARDS_1Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_1()
  __ZN15CAdvisorScoring28_ADVISOR_STAFF_HIRE_GUARDS_2Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_2()
  __ZN15CAdvisorScoring28_ADVISOR_STAFF_HIRE_GUARDS_3Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_3()
  __ZN15CAdvisorScoring32_ADVISOR_STAFF_SET_HANDYMAN_AREAEv   # CAdvisorScoring::_ADVISOR_STAFF_SET_HANDYMAN_AREA()
  __ZN15CAdvisorScoring32_ADVISOR_STAFF_SET_MECHANIC_AREAEv   # CAdvisorScoring::_ADVISOR_STAFF_SET_MECHANIC_AREA()
  __ZN15CAdvisorScoring33_ADVISOR_STAFF_HIRE_RESEARCHERS_2Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_2()
  __ZN15CAdvisorScoring34_ADVISOR_STAFF_HIRE_ENTERTAINERS_1Ev   # CAdvisorScoring::_ADVISOR_STAFF_HIRE_ENTERTAINERS_1()
)
