# The things' second batch (unit w130): CResearchLab::Init, SetupProjects, SetupCategories, CParkAnalyser::BuiltSomething, NumberOfStaffType,
# CBankAccount::MakeWithdrawal, CChallengeHQ::AddChallenge, ClearInvalidFollowups, IsSystemOn, OfferNextChallenge, CAdvisor::IsPlaying,
# FindHighestScoringSlotInQueue and CWeather::SetPrecipitationType. Every callee outside them (the world's getters, the time, the projects, the
# ride database, the sound, the rain, the message centre, the trace and the challenge hall's offer) is a fake in the unit file; gMainBalance is
# the real storage.
set(TPW_UNIT_Things2 src/ResearchLab_init.cpp src/ChallengeHQ_offer.cpp src/Things2_small.cpp src/MainBalance.cpp)
