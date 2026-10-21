# Unit v5767: CAdvisorScoring Load, Save, GetFirstResponseFor and three scorers (src/game/advisor/AdvisorScoring.cpp).
# The callees outside the unit are recording fakes in tests/unit_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL_KEEP
  __ZN15CAdvisorScoring26_ADVISOR_TUTORIAL_UI_PANELEv   # CAdvisorScoring::_ADVISOR_TUTORIAL_UI_PANEL()
  __ZN15CAdvisorScoring34_ADVISOR_PRICING_DRINKS_POOR_VALUEEv   # CAdvisorScoring::_ADVISOR_PRICING_DRINKS_POOR_VALUE()
  __ZN15CAdvisorScoring36_ADVISOR_GOLD_TICKET_NEAR_TO_XPROFITEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT()
  __ZN15CAdvisorScoring4LoadERPv   # CAdvisorScoring::Load(void*&)
  __ZNK15CAdvisorScoring19GetFirstResponseForE14AdvisorMessage   # CAdvisorScoring::GetFirstResponseFor(AdvisorMessage) const
  __ZNK15CAdvisorScoring4SaveERPv   # CAdvisorScoring::Save(void*&) const
  _gAdvisorNoneDrinksPoorValue
)
