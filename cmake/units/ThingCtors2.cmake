# The three singleton constructors and their inline parts (unit w122b). External callees are recording fakes.
set(TPW_UNIT_ThingCtors2 src/ParkAnalyser_ctor.cpp src/BankAccount_ctor.cpp src/ChallengeHQ_ctor.cpp src/MainBalance.cpp)
# The unit file includes the allocation counters from calllog.cpp.
set(TPW_UNIT_ThingCtors2_OWN_CALLLOG ON)
