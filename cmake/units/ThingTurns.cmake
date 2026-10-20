# The things' turns (unit w124): CWeather::UpdateTurn, CAdvisor::UpdateTurn, CChallengeHQ::UpdateTurn, CParkAnalyser::UpdateTurn, CParkAnalyserCell::Init,
# SubscriptionTree::insert_one and the nine companions (the constructor, GetType and destructor of CMsgWeather, CMsgWeatherWarning and CMsgChallenge).
# Every callee outside them (the time stamp operators, the funny time, the scorers, the advisor's queue, the challenge hall's helpers, the weather's
# helpers, the sound and rain calls, the message centre, the trace and the tree's node insertion) is a fake in the unit file; gMainBalance is the real storage.
set(TPW_UNIT_ThingTurns src/Weather_turn.cpp src/Advisor_turn.cpp src/ChallengeHQ_turn.cpp src/ParkAnalyser_turn.cpp src/msl_tree_insert.cpp src/ThingMessages.cpp src/MainBalance.cpp)
