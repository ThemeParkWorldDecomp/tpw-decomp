# The world's thing getters and the park-loop initialisers (unit w123b): CWorld::GetParkAnalyser, GetResearchLab (each with its const twin), GetWeather,
# GetBankAccount, GetChallengeHQ, CParkTime::GetFunnyTime, CParkAnalyser::Init and CChallengeHQ::Init. Every callee outside them (the asserts and traces, the
# world accessor, the time stamp imports, the message centre, the analyser cell, the challenge constructor, AddChallenge, ClearInvalidFollowups and the balance
# table's Init, Load and UnInit) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_WorldGetters src/World_getters.cpp src/ParkTime_funny.cpp src/ParkAnalyser_init.cpp src/ChallengeHQ_init.cpp src/ChallengeBalance.cpp src/MainBalance.cpp)
