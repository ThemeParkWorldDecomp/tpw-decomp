# Unit v5106: the advisor's gold ticket scorers and the tooltip tutorial scorer (src/game/advisor/AdvisorScoring.cpp).
# The callees outside the unit are recording fakes in tests/unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI_KEEP
  __ZN15CAdvisorScoring32_ADVISOR_GOLD_TICKET_EXPLANATIONEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_EXPLANATION()
  __ZN15CAdvisorScoring34_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPSEv   # CAdvisorScoring::_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS()
  __ZN15CAdvisorScoring38_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_aEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a()
  __ZN15CAdvisorScoring38_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_bEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b()
  __ZN15CAdvisorScoring38_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_cEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c()
  __ZN15CAdvisorScoring50_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_aEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a()
  __ZN15CAdvisorScoring50_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_bEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b()
  __ZN15CAdvisorScoring50_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_cEv   # CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c()
)
