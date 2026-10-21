# Unit v5108: the eight _ADVISOR_MAX_* scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2 src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2_KEEP
  __ZN15CAdvisorScoring19_ADVISOR_MAX_GUARDSEv   # CAdvisorScoring::_ADVISOR_MAX_GUARDS()
  __ZN15CAdvisorScoring21_ADVISOR_BOAT_ARRIVEDEv   # CAdvisorScoring::_ADVISOR_BOAT_ARRIVED()
  __ZN15CAdvisorScoring22_ADVISOR_MAX_GUARDS_V2Ev   # CAdvisorScoring::_ADVISOR_MAX_GUARDS_V2()
  __ZN15CAdvisorScoring22_ADVISOR_MAX_MECHANICSEv   # CAdvisorScoring::_ADVISOR_MAX_MECHANICS()
  __ZN15CAdvisorScoring24_ADVISOR_MAX_HANDYMEN_V2Ev   # CAdvisorScoring::_ADVISOR_MAX_HANDYMEN_V2()
  __ZN15CAdvisorScoring24_ADVISOR_MAX_RESEARCHERSEv   # CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS()
  __ZN15CAdvisorScoring25_ADVISOR_MAX_ENTERTAINERSEv   # CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS()
  __ZN15CAdvisorScoring25_ADVISOR_MAX_MECHANICS_V2Ev   # CAdvisorScoring::_ADVISOR_MAX_MECHANICS_V2()
  __ZN15CAdvisorScoring27_ADVISOR_MAX_RESEARCHERS_V2Ev   # CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS_V2()
  __ZN15CAdvisorScoring28_ADVISOR_MAX_ENTERTAINERS_V2Ev   # CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS_V2()
)
