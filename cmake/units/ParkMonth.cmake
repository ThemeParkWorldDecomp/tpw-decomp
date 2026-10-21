# Unit w161: the park analyser's messages, month end, admissions, thoughts and cell save (src/game/park/analyser.cpp: CParkAnalyser::ReceiveMessage,
# PersonPaidAdmission, UpdateEndOfMonth, GetThreeMostDominantThoughts, FinalDestroy, CParkAnalyserCell::Serialise and the start-up words) and the three
# messages with CBankAccount::GetBalance (src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/ui/MessageCentre.cpp). Every callee outside them is a recording fake in tests/unit_ParkMonth.cpp, which also
# defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_ParkMonth src/game/park/analyser.cpp src/game/buildings/ResearchLab.cpp src/game/buildings/StaffHQ.cpp
    src/game/park/BankAccount.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_ParkMonth_KEEP
  __ZN13CMsgStaffInfoC1E9StaffType14StaffSituation   # CMsgStaffInfo::CMsgStaffInfo(StaffType, StaffSituation)
  __ZN13CMsgStaffInfoD1Ev   # CMsgStaffInfo::~CMsgStaffInfo()
  __ZN13CParkAnalyser12FinalDestroyEv   # CParkAnalyser::FinalDestroy()
  __ZN13CParkAnalyser14ReceiveMessageERK4CMsg   # CParkAnalyser::ReceiveMessage(CMsg const&)
  __ZN13CParkAnalyser16UpdateEndOfMonthEv   # CParkAnalyser::UpdateEndOfMonth()
  __ZN13CParkAnalyser19PersonPaidAdmissionEi   # CParkAnalyser::PersonPaidAdmission(int)
  __ZN17CParkAnalyserCell9SerialiseEPviij   # CParkAnalyserCell::Serialise(void*, int, int, unsigned int)
  __ZN18CMsgAddToChallengeC1E13ChallengeTypei12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES1_I24IDXTYPEUniqueNameThingIdtE   # CMsgAddToChallenge::CMsgAddToChallenge(ChallengeType, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN18CMsgAddToChallengeD1Ev   # CMsgAddToChallenge::~CMsgAddToChallenge()
  __ZN21CMsgResearchCompletedC1E12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE11ResearchCat   # CMsgResearchCompleted::CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, ResearchCat)
  __ZN21CMsgResearchCompletedD1Ev   # CMsgResearchCompleted::~CMsgResearchCompleted()
  __ZNK12CBankAccount10GetBalanceEv   # CBankAccount::GetBalance() const
  __ZNK13CMsgStaffInfo7GetTypeEv   # CMsgStaffInfo::GetType() const
  __ZNK13CParkAnalyser28GetThreeMostDominantThoughtsERhS0_S0_   # CParkAnalyser::GetThreeMostDominantThoughts(unsigned char&, unsigned char&, unsigned char&) const
  __ZNK18CMsgAddToChallenge7GetTypeEv   # CMsgAddToChallenge::GetType() const
  __ZNK21CMsgResearchCompleted7GetTypeEv   # CMsgResearchCompleted::GetType() const
  _gAnalyserWord1034C19C
  _gAnalyserWord1034C1A0
)
