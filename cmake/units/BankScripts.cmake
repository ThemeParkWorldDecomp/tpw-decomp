# Unit w177: the bank account's loans, deposits and messages (src/BankAccount_loans.cpp), CChallengeBalance's destructor and two virtuals
# (src/ChallengeBalance.cpp) and CDelayedScript with its ScriptIdx serialise instances (src/DelayedScript.cpp). Every other callee is a recording fake
# in tests/unit_BankScripts.cpp; gMainBalance is the real storage (src/MainBalance.cpp), the script code array is defined in the unit file.
set(TPW_UNIT_BankScripts src/BankAccount_loans.cpp src/ChallengeBalance.cpp src/DelayedScript.cpp src/MainBalance.cpp platform/arena.cpp)
