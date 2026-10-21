# Unit v51003: CAdvisorRequest::Play and PlayWithoutScore (src/game/advisor/Advisor.cpp). The callees are
# recording fakes in tests/unit_CAdvisorRequest_Play.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisorRequest_Play src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisorRequest_Play_KEEP
  __ZN15CAdvisorRequest16PlayWithoutScoreEPji   # CAdvisorRequest::PlayWithoutScore(unsigned int*, int)
  __ZN15CAdvisorRequest4PlayEPji   # CAdvisorRequest::Play(unsigned int*, int)
  _gAdvisorNullThingId
)
