# Unit w173: the bank account's save, month and loans (src/game/park/BankAccount.cpp: CBankAccount::Serialise, MonthlyUpdate, GetLoanInfo, AnyLoanAvailable,
# PayOffLoan). CBankAccount::MakeWithdrawal (src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/Weather.cpp, src/game/park/analyser.cpp) is linked real; every other callee is a recording fake in tests/unit_BankAccount.cpp.
set(TPW_UNIT_BankAccount src/game/park/BankAccount.cpp src/game/advisor/Advisor.cpp src/game/park/Weather.cpp
    src/game/park/analyser.cpp platform/arena.cpp)
set(TPW_UNIT_BankAccount_KEEP
  __ZN12CBankAccount10PayOffLoanEj   # CBankAccount::PayOffLoan(unsigned int)
  __ZN12CBankAccount11GetLoanInfoEj   # CBankAccount::GetLoanInfo(unsigned int)
  __ZN12CBankAccount13MonthlyUpdateEv   # CBankAccount::MonthlyUpdate()
  __ZN12CBankAccount14MakeWithdrawalEj   # CBankAccount::MakeWithdrawal(unsigned int)
  __ZN12CBankAccount16AnyLoanAvailableEv   # CBankAccount::AnyLoanAvailable()
  __ZN12CBankAccount9SerialiseEPviij   # CBankAccount::Serialise(void*, int, int, unsigned int)
  __ZN13CParkAnalyser14BuiltSomethingEv   # CParkAnalyser::BuiltSomething()
  __ZN8CAdvisor29FindHighestScoringSlotInQueueEv   # CAdvisor::FindHighestScoringSlotInQueue()
  __ZN8CAdvisor9IsPlayingEv   # CAdvisor::IsPlaying()
  __ZN8CWeather20SetPrecipitationTypeE17PrecipitationType   # CWeather::SetPrecipitationType(PrecipitationType)
  __ZNK13CParkAnalyser17NumberOfStaffTypeEN6CThing10ThingModelE   # CParkAnalyser::NumberOfStaffType(CThing::ThingModel) const
)
