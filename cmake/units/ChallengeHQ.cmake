# Unit w175: the challenge hall's progress (CChallengeHQ::FinalDestroy, Serialise, AddToCounter, GetCurrentChallenge, ReceiveMessage, OfferFollowup,
# TellPlayerChallengeOffered, ChallengeAccepted, ChallengeDeclined, EndCurrentChallenge, WonCurrentChallenge and LostCurrentChallenge; src/game/park/ChallengeHQ.cpp).
# Every callee outside them (the base thing's and the challenges' serialise, the challenges' ids and win test, the save-file templates, the clock, the
# world's getters, the analyser's queries, the bank deposit, the message centre, the trace and the assert) is a fake in the unit file; gMainBalance is the
# real storage, and the message classes come from src/game/park/ChallengeHQ.cpp, src/game/park/Weather.cpp, src/game/ui/MessageCentre.cpp and src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp.
set(TPW_UNIT_ChallengeHQ src/game/park/ChallengeHQ.cpp src/game/park/Weather.cpp src/game/ui/MessageCentre.cpp
    src/game/buildings/ResearchLab.cpp src/game/buildings/StaffHQ.cpp src/game/park/BankAccount.cpp
    src/game/park/analyser.cpp src/game/balance/MainBalance.cpp platform/arena.cpp)
set(TPW_UNIT_ChallengeHQ_KEEP
  __ZN11CMsgWeatherC1Ei   # CMsgWeather::CMsgWeather(int)
  __ZN11CMsgWeatherD1Ev   # CMsgWeather::~CMsgWeather()
  __ZN12CChallengeHQ12AddToCounterE13ChallengeTypei12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES1_I24IDXTYPEUniqueNameThingIdtE   # CChallengeHQ::AddToCounter(ChallengeType, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN12CChallengeHQ12FinalDestroyEv   # CChallengeHQ::FinalDestroy()
  __ZN12CChallengeHQ13OfferFollowupEv   # CChallengeHQ::OfferFollowup()
  __ZN12CChallengeHQ14ReceiveMessageERK4CMsg   # CChallengeHQ::ReceiveMessage(CMsg const&)
  __ZN12CChallengeHQ17ChallengeAcceptedEv   # CChallengeHQ::ChallengeAccepted()
  __ZN12CChallengeHQ17ChallengeDeclinedEv   # CChallengeHQ::ChallengeDeclined()
  __ZN12CChallengeHQ19EndCurrentChallengeEv   # CChallengeHQ::EndCurrentChallenge()
  __ZN12CChallengeHQ19GetCurrentChallengeEv   # CChallengeHQ::GetCurrentChallenge()
  __ZN12CChallengeHQ19WonCurrentChallengeEv   # CChallengeHQ::WonCurrentChallenge()
  __ZN12CChallengeHQ20LostCurrentChallengeEv   # CChallengeHQ::LostCurrentChallenge()
  __ZN12CChallengeHQ26TellPlayerChallengeOfferedEP10CChallengeb   # CChallengeHQ::TellPlayerChallengeOffered(CChallenge*, bool)
  __ZN12CChallengeHQ9SerialiseEPviij   # CChallengeHQ::Serialise(void*, int, int, unsigned int)
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CMsgChallengeC1Ei14AdvisorMessagebiiiii12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES3_9StaffType   # CMsgChallenge::CMsgChallenge(int, AdvisorMessage, bool, int, int, int, int, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, StaffType)
  __ZN13CMsgChallengeD1Ev   # CMsgChallenge::~CMsgChallenge()
  __ZN13CMsgStaffInfoC1E9StaffType14StaffSituation   # CMsgStaffInfo::CMsgStaffInfo(StaffType, StaffSituation)
  __ZN13CMsgStaffInfoD1Ev   # CMsgStaffInfo::~CMsgStaffInfo()
  __ZN18CMsgAddToChallengeC1E13ChallengeTypei12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES1_I24IDXTYPEUniqueNameThingIdtE   # CMsgAddToChallenge::CMsgAddToChallenge(ChallengeType, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN18CMsgAddToChallengeD1Ev   # CMsgAddToChallenge::~CMsgAddToChallenge()
  __ZN18CMsgWeatherWarningC1Eii   # CMsgWeatherWarning::CMsgWeatherWarning(int, int)
  __ZN18CMsgWeatherWarningD1Ev   # CMsgWeatherWarning::~CMsgWeatherWarning()
  __ZN21CMsgResearchCompletedC1E12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE11ResearchCat   # CMsgResearchCompleted::CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, ResearchCat)
  __ZN21CMsgResearchCompletedD1Ev   # CMsgResearchCompleted::~CMsgResearchCompleted()
  __ZNK11CMsgWeather7GetTypeEv   # CMsgWeather::GetType() const
  __ZNK12CBankAccount10GetBalanceEv   # CBankAccount::GetBalance() const
  __ZNK13CMsgChallenge7GetTypeEv   # CMsgChallenge::GetType() const
  __ZNK13CMsgStaffInfo7GetTypeEv   # CMsgStaffInfo::GetType() const
  __ZNK18CMsgAddToChallenge7GetTypeEv   # CMsgAddToChallenge::GetType() const
  __ZNK18CMsgWeatherWarning7GetTypeEv   # CMsgWeatherWarning::GetType() const
  __ZNK21CMsgResearchCompleted7GetTypeEv   # CMsgResearchCompleted::GetType() const
  _gMainBalance
  _gMainBalanceDescriptions
)
