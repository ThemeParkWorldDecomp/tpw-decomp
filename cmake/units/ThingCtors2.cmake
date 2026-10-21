# The three singleton constructors and their inline parts (unit w122b). External callees are recording fakes.
set(TPW_UNIT_ThingCtors2 src/game/park/analyser.cpp src/game/park/BankAccount.cpp src/game/park/ChallengeHQ.cpp
    src/game/balance/MainBalance.cpp)
set(TPW_UNIT_ThingCtors2_KEEP
  __ZN12CBankAccount15SetAdmissionFeeEj   # CBankAccount::SetAdmissionFee(unsigned int)
  __ZN12CBankAccountC1Ev   # CBankAccount::CBankAccount()
  __ZN12CChallengeHQC1Ev   # CChallengeHQ::CChallengeHQ()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CParkAnalyserC1Ev   # CParkAnalyser::CParkAnalyser()
  _gMainBalance
  _gMainBalanceDescriptions
)
# The unit file includes the allocation counters from calllog.cpp.
set(TPW_UNIT_ThingCtors2_OWN_CALLLOG ON)
