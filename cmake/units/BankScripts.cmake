# Unit w177: the bank account's loans, deposits and messages (src/game/park/BankAccount.cpp), CChallengeBalance's destructor and two virtuals
# (src/game/park/ChallengeHQ.cpp) and CDelayedScript with its ScriptIdx serialise instances (src/game/DelayedScript.cpp). Every other callee is a recording fake
# in tests/unit_BankScripts.cpp; gMainBalance is the real storage (src/game/balance/MainBalance.cpp), the script code array is defined in the unit file.
set(TPW_UNIT_BankScripts src/game/park/BankAccount.cpp src/game/park/ChallengeHQ.cpp src/game/DelayedScript.cpp
    src/game/balance/MainBalance.cpp platform/arena.cpp)
set(TPW_UNIT_BankScripts_KEEP
  __Z7LOADVARIN14CDelayedScript9ScriptIdxEEiPvPT_PKci   # int LOADVAR<CDelayedScript::ScriptIdx>(void*, CDelayedScript::ScriptIdx*, char const*, int)
  __Z7SAVEVARIN14CDelayedScript9ScriptIdxEEiPvPT_PKci   # int SAVEVAR<CDelayedScript::ScriptIdx>(void*, CDelayedScript::ScriptIdx*, char const*, int)
  __ZN12CBankAccount11MakeDepositEj   # CBankAccount::MakeDeposit(unsigned int)
  __ZN12CBankAccount12ApplyForLoanEj   # CBankAccount::ApplyForLoan(unsigned int)
  __ZN12CBankAccount12FinalDestroyEv   # CBankAccount::FinalDestroy()
  __ZN12CBankAccount12PayAdmissionEv   # CBankAccount::PayAdmission()
  __ZN12CBankAccount14ReceiveMessageERK4CMsg   # CBankAccount::ReceiveMessage(CMsg const&)
  __ZN12CBankAccount15BeenInTheRedForEv   # CBankAccount::BeenInTheRedFor()
  __ZN12CBankAccount22GetNumberOfLoansBoughtEv   # CBankAccount::GetNumberOfLoansBought()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN14CDelayedScript15SetSpriteScriptENS_9ScriptIdxE   # CDelayedScript::SetSpriteScript(CDelayedScript::ScriptIdx)
  __ZN14CDelayedScript16KillSpriteScriptEv   # CDelayedScript::KillSpriteScript()
  __ZN14CDelayedScript17SpawnSpriteScriptENS_9ScriptIdxEii   # CDelayedScript::SpawnSpriteScript(CDelayedScript::ScriptIdx, int, int)
  __ZN14CDelayedScript18SetServiceIntervalEf   # CDelayedScript::SetServiceInterval(float)
  __ZN14CDelayedScript19IsRunningThisScriptENS_9ScriptIdxE   # CDelayedScript::IsRunningThisScript(CDelayedScript::ScriptIdx)
  __ZN14CDelayedScript20ProcessSpriteScriptsEv   # CDelayedScript::ProcessSpriteScripts()
  __ZN14CDelayedScript9SerialiseEPviij   # CDelayedScript::Serialise(void*, int, int, unsigned int)
  __ZN14CDelayedScriptC1Ev   # CDelayedScript::CDelayedScript()
  __ZN17CChallengeBalance21GetBalanceDescriptionEi   # CChallengeBalance::GetBalanceDescription(int)
  __ZN17CChallengeBalance21GetBalanceValuesStartEv   # CChallengeBalance::GetBalanceValuesStart()
  __ZN17CChallengeBalanceD1Ev   # CChallengeBalance::~CChallengeBalance()
  __ZNK12CBankAccount17GetProfitThisYearEv   # CBankAccount::GetProfitThisYear() const
  __ZNK12CBankAccount17OutstandingOnLoanEj   # CBankAccount::OutstandingOnLoan(unsigned int) const
  __ZNK12CBankAccount20GetCheapestLoanTakenEv   # CBankAccount::GetCheapestLoanTaken() const
  __ZNK12CBankAccount23TotalOutstandingOnLoansEv   # CBankAccount::TotalOutstandingOnLoans() const
  _gChallengeBalanceDescriptions
  _gMainBalance
  _gMainBalanceDescriptions
)
