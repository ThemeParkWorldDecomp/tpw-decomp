# Unit v5896: twelve advisor scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_KEEP
  __ZN15CAdvisorScoring19_ADVISOR_WAGES_HIGHEv   # CAdvisorScoring::_ADVISOR_WAGES_HIGH()
  __ZN15CAdvisorScoring21_ADVISOR_MAX_HANDYMENEv   # CAdvisorScoring::_ADVISOR_MAX_HANDYMEN()
  __ZN15CAdvisorScoring22_ADVISOR_LOTS_SECURITYEv   # CAdvisorScoring::_ADVISOR_LOTS_SECURITY()
  __ZN15CAdvisorScoring23_ADVISOR_LOTS_MECHANICSEv   # CAdvisorScoring::_ADVISOR_LOTS_MECHANICS()
  __ZN15CAdvisorScoring25_ADVISOR_LOTS_RESEARCHERSEv   # CAdvisorScoring::_ADVISOR_LOTS_RESEARCHERS()
  __ZN15CAdvisorScoring26_ADVISOR_QUEUE_UNCONNECTEDEv   # CAdvisorScoring::_ADVISOR_QUEUE_UNCONNECTED()
  __ZN15CAdvisorScoring28_ADVISOR_FEATURES_NO_TOILETSEv   # CAdvisorScoring::_ADVISOR_FEATURES_NO_TOILETS()
  __ZN15CAdvisorScoring31_ADVISOR_WEALTH_HIGH_REPAY_LOANEv   # CAdvisorScoring::_ADVISOR_WEALTH_HIGH_REPAY_LOAN()
  __ZN15CAdvisorScoring32_ADVISOR_FEATURES_BUILD_FEATURESEv   # CAdvisorScoring::_ADVISOR_FEATURES_BUILD_FEATURES()
  __ZN15CAdvisorScoring33_ADVISOR_PRICING_TICKET_TOO_CHEAPEv   # CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_CHEAP()
  __ZN15CAdvisorScoring36_ADVISOR_FEATURES_NOT_ENOUGH_TOILETSEv   # CAdvisorScoring::_ADVISOR_FEATURES_NOT_ENOUGH_TOILETS()
  __ZN15CAdvisorScoring37_ADVISOR_PRICING_TICKET_TOO_EXPENSIVEEv   # CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_EXPENSIVE()
  _gAdvisorNoRideQueue
)
