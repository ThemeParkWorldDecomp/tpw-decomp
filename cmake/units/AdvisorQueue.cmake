# Advisor.cpp: request queue only; scoring queries and construction are recording fakes.
set(TPW_UNIT_AdvisorQueue src/game/advisor/Advisor.cpp)
set(TPW_UNIT_AdvisorQueue_KEEP
  __Z10TheScoringv   # TheScoring()
  __ZN8CAdvisor12QueueRequestERK15CAdvisorRequest   # CAdvisor::QueueRequest(CAdvisorRequest const&)
  __ZN8CAdvisor27FindFirstInvalidSlotInQueueEv   # CAdvisor::FindFirstInvalidSlotInQueue()
  __ZN8CAdvisor28FindLowestScoringSlotInQueueEv   # CAdvisor::FindLowestScoringSlotInQueue()
  __ZNK8CAdvisor13CanIQueueThisE14AdvisorMessageb   # CAdvisor::CanIQueueThis(AdvisorMessage, bool) const
)
