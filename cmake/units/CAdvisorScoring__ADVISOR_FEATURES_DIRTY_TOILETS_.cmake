# Unit v5081: six advisor scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.cpp, which also defines the world pointer.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_ src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS__KEEP
  __ZN15CAdvisorScoring32_ADVISOR_FEATURES_POOR_SELECTIONEv   # CAdvisorScoring::_ADVISOR_FEATURES_POOR_SELECTION()
  __ZN15CAdvisorScoring33_ADVISOR_FEATURES_DIRTY_TOILETS_1Ev   # CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_1()
  __ZN15CAdvisorScoring33_ADVISOR_FEATURES_DIRTY_TOILETS_2Ev   # CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_2()
  __ZN15CAdvisorScoring33_ADVISOR_FEATURES_DIRTY_TOILETS_3Ev   # CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_3()
  __ZN15CAdvisorScoring33_ADVISOR_FEATURES_DIRTY_TOILETS_4Ev   # CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_4()
  __ZN15CAdvisorScoring39_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWSEv   # CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS()
  _gAdvisorNoToilet1
  _gAdvisorNoToilet2
  _gAdvisorNoToilet3
  _gAdvisorNoToilet4
)
