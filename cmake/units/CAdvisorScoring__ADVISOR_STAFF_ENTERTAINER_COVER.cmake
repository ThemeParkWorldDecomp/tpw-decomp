# Unit v5118: the advisor's staff scorers (src/game/advisor/AdvisorScoring.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.cpp, which also defines gGameWorld and gGameWorldRef.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER_KEEP
  __ZN15CAdvisorScoring27_ADVISOR_STAFF_TRAIN_GUARDSEv   # CAdvisorScoring::_ADVISOR_STAFF_TRAIN_GUARDS()
  __ZN15CAdvisorScoring29_ADVISOR_STAFF_SET_GUARD_AREAEv   # CAdvisorScoring::_ADVISOR_STAFF_SET_GUARD_AREA()
  __ZN15CAdvisorScoring29_ADVISOR_STAFF_TRAIN_HANDYMENEv   # CAdvisorScoring::_ADVISOR_STAFF_TRAIN_HANDYMEN()
  __ZN15CAdvisorScoring30_ADVISOR_STAFF_TRAIN_MECHANICSEv   # CAdvisorScoring::_ADVISOR_STAFF_TRAIN_MECHANICS()
  __ZN15CAdvisorScoring33_ADVISOR_STAFF_GUARD_BUILD_CAMERAEv   # CAdvisorScoring::_ADVISOR_STAFF_GUARD_BUILD_CAMERA()
  __ZN15CAdvisorScoring33_ADVISOR_STAFF_TRAIN_ENTERTAINERSEv   # CAdvisorScoring::_ADVISOR_STAFF_TRAIN_ENTERTAINERS()
  __ZN15CAdvisorScoring35_ADVISOR_STAFF_ENTERTAINER_COVERAGEEv   # CAdvisorScoring::_ADVISOR_STAFF_ENTERTAINER_COVERAGE()
  __ZN15CAdvisorScoring35_ADVISOR_STAFF_SET_ENTERTAINER_AREAEv   # CAdvisorScoring::_ADVISOR_STAFF_SET_ENTERTAINER_AREA()
)
