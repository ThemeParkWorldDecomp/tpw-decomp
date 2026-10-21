# Unit w155: the park analyser's staff queries (src/game/park/analyser.cpp; 13 functions). Every callee outside the unit (the world accessor and its
# used-thing walk, the assertion, CStaff::TooTiredToWork and HasPatrolArea, CStaffDB::GetNumStaffOfType, CParkTime::GetFunnyTime and
# TbTimeStamp::operator-) is a recording fake in tests/unit_ParkStaff.cpp; gMainBalance is the real storage.
set(TPW_UNIT_ParkStaff src/game/park/analyser.cpp src/game/balance/MainBalance.cpp platform/arena.cpp)
set(TPW_UNIT_ParkStaff_KEEP
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CParkAnalyser12HiredSomeoneEv   # CParkAnalyser::HiredSomeone()
  __ZNK13CParkAnalyser13NumberOfStaffEv   # CParkAnalyser::NumberOfStaff() const
  __ZNK13CParkAnalyser20MaxResearcherAbilityEv   # CParkAnalyser::MaxResearcherAbility() const
  __ZNK13CParkAnalyser22GetStaffHappinessLevelEj   # CParkAnalyser::GetStaffHappinessLevel(unsigned int) const
  __ZNK13CParkAnalyser23AverageHappinessOfStaffEv   # CParkAnalyser::AverageHappinessOfStaff() const
  __ZNK13CParkAnalyser24CurrentResearcherAbilityEv   # CParkAnalyser::CurrentResearcherAbility() const
  __ZNK13CParkAnalyser25AverageQualityOfStaffTypeEN6CThing10ThingModelE   # CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel) const
  __ZNK13CParkAnalyser26GetDaysSinceLastStaffHiredEv   # CParkAnalyser::GetDaysSinceLastStaffHired() const
  __ZNK13CParkAnalyser27AverageHappinessOfStaffTypeEN6CThing10ThingModelE   # CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel) const
  __ZNK13CParkAnalyser27GetDaysSinceLastObjectBuiltEv   # CParkAnalyser::GetDaysSinceLastObjectBuilt() const
  __ZNK13CParkAnalyser27NumberOfStaffTooTiredToWorkEv   # CParkAnalyser::NumberOfStaffTooTiredToWork() const
  __ZNK13CParkAnalyser31CurrentNumberOfStaffTypeForHireE9StaffType   # CParkAnalyser::CurrentNumberOfStaffTypeForHire(StaffType) const
  __ZNK13CParkAnalyser31NumberOfStaffTypeWithPatrolAreaEN6CThing10ThingModelE   # CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel) const
  _gMainBalance
  _gMainBalanceDescriptions
)
