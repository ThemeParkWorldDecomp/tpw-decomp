# Unit v5937: SAVEVARSTRUCT<AdvisorHistory>, CAdvisorScoring::LastMinuteCheck, CAdvisorRequest::Serialise (src/game/advisor/Advisor.cpp, src/game/advisor/AdvisorScoring.cpp).
# The callees outside the unit are recording fakes in tests/unit_SAVEVARSTRUCT_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_SAVEVARSTRUCT_2 src/game/advisor/Advisor.cpp src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_SAVEVARSTRUCT_2_KEEP
  __Z13SAVEVARSTRUCTI19AdvisorMessageStateEiPvPT_PKci   # int SAVEVARSTRUCT<AdvisorMessageState>(void*, AdvisorMessageState*, char const*, int)
  __ZN15CAdvisorRequest9SerialiseEPviij   # CAdvisorRequest::Serialise(void*, int, int, unsigned int)
  __ZN15CAdvisorScoring15LastMinuteCheckE14AdvisorMessageR12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtERj   # CAdvisorScoring::LastMinuteCheck(AdvisorMessage, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&, unsigned int&)
)
