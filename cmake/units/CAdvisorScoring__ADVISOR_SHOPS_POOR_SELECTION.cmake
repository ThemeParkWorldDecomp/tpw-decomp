# Unit v5939: nine advisor scorers (src/game/advisor/AdvisorScoring.cpp).
# The callees outside the unit are recording fakes in tests/unit_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.cpp, which also defines gGameWorld.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION_KEEP
  __ZN15CAdvisorScoring26_ADVISOR_RIDES_BUILD_RIDESEv   # CAdvisorScoring::_ADVISOR_RIDES_BUILD_RIDES()
  __ZN15CAdvisorScoring26_ADVISOR_RIDES_NO_UPGRADESEv   # CAdvisorScoring::_ADVISOR_RIDES_NO_UPGRADES()
  __ZN15CAdvisorScoring26_ADVISOR_SHOPS_BUILD_SHOPSEv   # CAdvisorScoring::_ADVISOR_SHOPS_BUILD_SHOPS()
  __ZN15CAdvisorScoring28_ADVISOR_RIDES_MORE_UPGRADESEv   # CAdvisorScoring::_ADVISOR_RIDES_MORE_UPGRADES()
  __ZN15CAdvisorScoring28_ADVISOR_RIDES_RESEARCH_MOREEv   # CAdvisorScoring::_ADVISOR_RIDES_RESEARCH_MORE()
  __ZN15CAdvisorScoring28_ADVISOR_SHOPS_RESEARCH_MOREEv   # CAdvisorScoring::_ADVISOR_SHOPS_RESEARCH_MORE()
  __ZN15CAdvisorScoring29_ADVISOR_SHOPS_POOR_SELECTIONEv   # CAdvisorScoring::_ADVISOR_SHOPS_POOR_SELECTION()
  __ZN15CAdvisorScoring31_ADVISOR_SHOPS_BUILD_MORE_SHOPSEv   # CAdvisorScoring::_ADVISOR_SHOPS_BUILD_MORE_SHOPS()
  __ZN15CAdvisorScoring33_ADVISOR_SIDESHOWS_POOR_SELECTIONEv   # CAdvisorScoring::_ADVISOR_SIDESHOWS_POOR_SELECTION()
)
