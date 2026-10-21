# Unit v5L05: LOADVAR and SAVEVAR of AdvisorMessage (src/game/advisor/Advisor.cpp). The callees outside the unit
# are recording fakes in tests/unit_AdvisorMessageVar.cpp, which also defines gSaveLoad.
set(TPW_UNIT_AdvisorMessageVar src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_AdvisorMessageVar_KEEP
  __Z7LOADVARI14AdvisorMessageEiPvPT_PKci   # int LOADVAR<AdvisorMessage>(void*, AdvisorMessage*, char const*, int)
  __Z7SAVEVARI14AdvisorMessageEiPvPT_PKci   # int SAVEVAR<AdvisorMessage>(void*, AdvisorMessage*, char const*, int)
)
