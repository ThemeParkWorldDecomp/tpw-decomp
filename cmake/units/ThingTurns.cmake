# The things' turns (unit w124): CWeather::UpdateTurn, CAdvisor::UpdateTurn, CChallengeHQ::UpdateTurn, CParkAnalyser::UpdateTurn, CParkAnalyserCell::Init,
# SubscriptionTree::insert_one and the nine companions (the constructor, GetType and destructor of CMsgWeather, CMsgWeatherWarning and CMsgChallenge).
# Every callee outside them (the time stamp operators, the funny time, the scorers, the advisor's queue, the challenge hall's helpers, the weather's
# helpers, the sound and rain calls, the message centre, the trace and the tree's node insertion) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_ThingTurns src/game/park/Weather.cpp src/game/advisor/Advisor.cpp src/game/park/ChallengeHQ.cpp
    src/game/park/analyser.cpp src/game/MslSupport.cpp src/game/ui/MessageCentre.cpp src/game/balance/MainBalance.cpp)
set(TPW_UNIT_ThingTurns_KEEP
  __ZN11CMsgWeatherC1Ei   # CMsgWeather::CMsgWeather(int)
  __ZN11CMsgWeatherD1Ev   # CMsgWeather::~CMsgWeather()
  __ZN12CChallengeHQ10UpdateTurnEv   # CChallengeHQ::UpdateTurn()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CMsgChallengeC1Ei14AdvisorMessagebiiiii12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES3_9StaffType   # CMsgChallenge::CMsgChallenge(int, AdvisorMessage, bool, int, int, int, int, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, StaffType)
  __ZN13CMsgChallengeD1Ev   # CMsgChallenge::~CMsgChallenge()
  __ZN13CParkAnalyser10UpdateTurnEv   # CParkAnalyser::UpdateTurn()
  __ZN16SubscriptionTree10insert_oneERK13CSubscription   # SubscriptionTree::insert_one(CSubscription const&)
  __ZN17CParkAnalyserCell4InitEiiii   # CParkAnalyserCell::Init(int, int, int, int)
  __ZN18CMsgWeatherWarningC1Eii   # CMsgWeatherWarning::CMsgWeatherWarning(int, int)
  __ZN18CMsgWeatherWarningD1Ev   # CMsgWeatherWarning::~CMsgWeatherWarning()
  __ZN8CAdvisor10UpdateTurnEv   # CAdvisor::UpdateTurn()
  __ZN8CWeather10UpdateTurnEv   # CWeather::UpdateTurn()
  __ZNK11CMsgWeather7GetTypeEv   # CMsgWeather::GetType() const
  __ZNK13CMsgChallenge7GetTypeEv   # CMsgChallenge::GetType() const
  __ZNK18CMsgWeatherWarning7GetTypeEv   # CMsgWeatherWarning::GetType() const
  _gMainBalance
  _gMainBalanceDescriptions
)
