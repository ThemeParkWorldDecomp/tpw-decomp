# Unit w103: the advisor request's constructors and SetScore, CAdvisor::CancelMessage, PlayLastTutorialMessage and GetTypePlaying, and
# CWorld::GetAdvisor. The queue's own functions (src/AdvisorQueue.cpp, unit w100) run for real; scoring, the clock, ADV_Slap and
# GAME_ASSERTMSG are recording fakes in the unit file.
set(TPW_UNIT_AdvisorRequest src/AdvisorRequest.cpp src/World_GetAdvisor.cpp src/AdvisorQueue.cpp)
