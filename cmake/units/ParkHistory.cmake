# Unit w157: the analyser's history rings and value templates (src/ParkAnalyser_history.cpp: CHistory SetAtEnd, GetValue, HowFarBack, GetSumOfLast,
# GetAverageOfLast, SetFromTemp, GetHighestValue, GetLowestValue and CParkAnalyserVal<int> Get and Set). GAME_ASSERTMSG is a recording fake in
# tests/unit_ParkHistory.cpp, which also defines gWorld; CTimeSig::SetTimeSig is the inline original.
set(TPW_UNIT_ParkHistory src/ParkAnalyser_history.cpp platform/arena.cpp)
