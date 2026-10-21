# The things' second batch (unit w130): CResearchLab::Init, SetupProjects, SetupCategories, CParkAnalyser::BuiltSomething, NumberOfStaffType,
# CBankAccount::MakeWithdrawal, CChallengeHQ::AddChallenge, ClearInvalidFollowups, IsSystemOn, OfferNextChallenge, CAdvisor::IsPlaying,
# FindHighestScoringSlotInQueue and CWeather::SetPrecipitationType. Every callee outside them (the world's getters, the time, the projects, the
# ride database, the sound, the rain, the message centre, the trace and the challenge hall's offer) is a fake in the unit file; gMainBalance is
# the real storage.
set(TPW_UNIT_Things2 src/game/buildings/ResearchLab.cpp src/game/park/ChallengeHQ.cpp src/game/advisor/Advisor.cpp
    src/game/park/BankAccount.cpp src/game/park/Weather.cpp src/game/park/analyser.cpp src/game/balance/MainBalance.cpp)
set(TPW_UNIT_Things2_KEEP
  __ZN12CBankAccount14MakeWithdrawalEj   # CBankAccount::MakeWithdrawal(unsigned int)
  __ZN12CChallengeHQ10IsSystemOnEv   # CChallengeHQ::IsSystemOn()
  __ZN12CChallengeHQ12AddChallengeER10CChallenge   # CChallengeHQ::AddChallenge(CChallenge&)
  __ZN12CChallengeHQ18OfferNextChallengeEb   # CChallengeHQ::OfferNextChallenge(bool)
  __ZN12CChallengeHQ21ClearInvalidFollowupsEv   # CChallengeHQ::ClearInvalidFollowups()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN12CResearchLab13SetupProjectsEv   # CResearchLab::SetupProjects()
  __ZN12CResearchLab15SetupCategoriesEv   # CResearchLab::SetupCategories()
  __ZN12CResearchLab4InitEv   # CResearchLab::Init()
  __ZN13CParkAnalyser14BuiltSomethingEv   # CParkAnalyser::BuiltSomething()
  __ZN8CAdvisor29FindHighestScoringSlotInQueueEv   # CAdvisor::FindHighestScoringSlotInQueue()
  __ZN8CAdvisor9IsPlayingEv   # CAdvisor::IsPlaying()
  __ZN8CWeather20SetPrecipitationTypeE17PrecipitationType   # CWeather::SetPrecipitationType(PrecipitationType)
  __ZNK13CParkAnalyser17NumberOfStaffTypeEN6CThing10ThingModelE   # CParkAnalyser::NumberOfStaffType(CThing::ThingModel) const
  _gMainBalance
  _gMainBalanceDescriptions
)
