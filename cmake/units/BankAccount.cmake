# Unit w173: the bank account's save, month and loans (src/BankAccount.cpp: CBankAccount::Serialise, MonthlyUpdate, GetLoanInfo, AnyLoanAvailable,
# PayOffLoan). CBankAccount::MakeWithdrawal (src/Things2_small.cpp) is linked real; every other callee is a recording fake in tests/unit_BankAccount.cpp.
set(TPW_UNIT_BankAccount src/BankAccount.cpp src/Things2_small.cpp platform/arena.cpp)
