# The world's thing getters and the park-loop initialisers (unit w123b): CWorld::GetParkAnalyser, GetResearchLab (each with its const twin), GetWeather,
# GetBankAccount, GetChallengeHQ, CParkTime::GetFunnyTime, CParkAnalyser::Init and CChallengeHQ::Init. Every callee outside them (the asserts and traces, the
# world accessor, the time stamp imports, the message centre, the analyser cell, the challenge constructor, AddChallenge, ClearInvalidFollowups and the balance
# table's Init, Load and UnInit) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_WorldGetters src/game/World.cpp src/game/people/ParkTime.cpp src/game/park/analyser.cpp
    src/game/park/ChallengeHQ.cpp src/game/balance/MainBalance.cpp)
set(TPW_UNIT_WorldGetters_KEEP
  __ZN12CChallengeHQ4InitEv   # CChallengeHQ::Init()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CParkAnalyser4InitEv   # CParkAnalyser::Init()
  __ZN17CChallengeBalance21GetBalanceDescriptionEi   # CChallengeBalance::GetBalanceDescription(int)
  __ZN17CChallengeBalance21GetBalanceValuesStartEv   # CChallengeBalance::GetBalanceValuesStart()
  __ZN17CChallengeBalanceD1Ev   # CChallengeBalance::~CChallengeBalance()
  __ZN6CWorld10GetWeatherEv   # CWorld::GetWeather()
  __ZN6CWorld14GetBankAccountEv   # CWorld::GetBankAccount()
  __ZN6CWorld14GetChallengeHQEv   # CWorld::GetChallengeHQ()
  __ZN6CWorld14GetResearchLabEv   # CWorld::GetResearchLab()
  __ZN6CWorld15GetParkAnalyserEv   # CWorld::GetParkAnalyser()
  __ZNK6CWorld14GetResearchLabEv   # CWorld::GetResearchLab() const
  __ZNK6CWorld15GetParkAnalyserEv   # CWorld::GetParkAnalyser() const
  __ZNK9CParkTime12GetFunnyTimeEPiS0_S0_S0_S0_S0_S0_S0_   # CParkTime::GetFunnyTime(int*, int*, int*, int*, int*, int*, int*, int*) const
  _gChallengeBalanceDescriptions
  _gMainBalance
  _gMainBalanceDescriptions
)
