# Unit w95: the advisor's message scoring (CAdvisorScoring's constructor, destructor and ten queries) and CAdvisorBalance (the advisor's
# balance table: two virtuals and the generated description rows). The message table is generated data. The balance reader (Init, Load,
# UnInit), the trace output and the game type constructor are fakes in the unit file; the three scorers the table's pointers use are fakes too.
set(TPW_UNIT_AdvisorScoring src/game/advisor/Advisor.cpp src/game/advisor/AdvisorScoring.cpp
    ${TPW_GENERATED_DIR}/AdvisorScoring_table.cpp ${TPW_GENERATED_DIR}/AdvisorBalance_table.cpp)
set(TPW_UNIT_AdvisorScoring_KEEP
  __ZN15CAdvisorBalance21GetBalanceDescriptionEi   # CAdvisorBalance::GetBalanceDescription(int)
  __ZN15CAdvisorBalance21GetBalanceValuesStartEv   # CAdvisorBalance::GetBalanceValuesStart()
  __ZN15CAdvisorScoring11GetScoreForE14AdvisorMessage   # CAdvisorScoring::GetScoreFor(AdvisorMessage)
  __ZN15CAdvisorScoringC1Ev   # CAdvisorScoring::CAdvisorScoring()
  __ZN15CAdvisorScoringD1Ev   # CAdvisorScoring::~CAdvisorScoring()
  __ZNK15CAdvisorScoring11SayOnlyOnceE14AdvisorMessage   # CAdvisorScoring::SayOnlyOnce(AdvisorMessage) const
  __ZNK15CAdvisorScoring13GetCyclingForE14AdvisorMessage   # CAdvisorScoring::GetCyclingFor(AdvisorMessage) const
  __ZNK15CAdvisorScoring13GetMinTimeForE14AdvisorMessage   # CAdvisorScoring::GetMinTimeFor(AdvisorMessage) const
  __ZNK15CAdvisorScoring18GetMessageCategoryE14AdvisorMessage   # CAdvisorScoring::GetMessageCategory(AdvisorMessage) const
  __ZNK15CAdvisorScoring18GetNumResponsesForE14AdvisorMessage   # CAdvisorScoring::GetNumResponsesFor(AdvisorMessage) const
  __ZNK15CAdvisorScoring22IsThisATutorialMessageE14AdvisorMessage   # CAdvisorScoring::IsThisATutorialMessage(AdvisorMessage) const
  __ZNK15CAdvisorScoring26GetMaxDuplicatesInQueueForE14AdvisorMessage   # CAdvisorScoring::GetMaxDuplicatesInQueueFor(AdvisorMessage) const
  __ZNK15CAdvisorScoring27IsThisValidInTheCurrentModeE14AdvisorMessage   # CAdvisorScoring::IsThisValidInTheCurrentMode(AdvisorMessage) const
  __ZNK15CAdvisorScoring29HasThisBeenKilledTooManyTimesE14AdvisorMessagei   # CAdvisorScoring::HasThisBeenKilledTooManyTimes(AdvisorMessage, int) const
)
