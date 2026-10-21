# Unit w103: the advisor request's constructors and SetScore, CAdvisor::CancelMessage, PlayLastTutorialMessage and GetTypePlaying, and
# CWorld::GetAdvisor. The queue's own functions (src/game/advisor/Advisor.cpp, unit w100) run for real; scoring, the clock, ADV_Slap and
# GAME_ASSERTMSG are recording fakes in the unit file.
set(TPW_UNIT_AdvisorRequest src/game/advisor/Advisor.cpp src/game/World.cpp)
set(TPW_UNIT_AdvisorRequest_KEEP
  __Z10TheScoringv   # TheScoring()
  __ZN15CAdvisorRequest8SetScoreEi   # CAdvisorRequest::SetScore(int)
  __ZN15CAdvisorRequestC1E14AdvisorMessage12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEjibi   # CAdvisorRequest::CAdvisorRequest(AdvisorMessage, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, unsigned int, int, bool, int)
  __ZN15CAdvisorRequestC1ERKS_   # CAdvisorRequest::CAdvisorRequest(CAdvisorRequest const&)
  __ZN6CWorld10GetAdvisorEv   # CWorld::GetAdvisor()
  __ZN8CAdvisor12QueueRequestERK15CAdvisorRequest   # CAdvisor::QueueRequest(CAdvisorRequest const&)
  __ZN8CAdvisor13CancelMessageE14AdvisorMessage   # CAdvisor::CancelMessage(AdvisorMessage)
  __ZN8CAdvisor14GetTypePlayingEv   # CAdvisor::GetTypePlaying()
  __ZN8CAdvisor23PlayLastTutorialMessageEv   # CAdvisor::PlayLastTutorialMessage()
  __ZN8CAdvisor27FindFirstInvalidSlotInQueueEv   # CAdvisor::FindFirstInvalidSlotInQueue()
  __ZN8CAdvisor28FindLowestScoringSlotInQueueEv   # CAdvisor::FindLowestScoringSlotInQueue()
  __ZNK8CAdvisor13CanIQueueThisE14AdvisorMessageb   # CAdvisor::CanIQueueThis(AdvisorMessage, bool) const
)
