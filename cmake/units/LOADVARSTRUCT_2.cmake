# Unit v5649: LOADVARSTRUCT<AdvisorHistory> (src/game/advisor/Advisor.cpp). The callees outside the unit are recording fakes in
# tests/unit_LOADVARSTRUCT_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LOADVARSTRUCT_2 src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_LOADVARSTRUCT_2_KEEP
  __Z13LOADVARSTRUCTI19AdvisorMessageStateEiPvPT_PKci   # int LOADVARSTRUCT<AdvisorMessageState>(void*, AdvisorMessageState*, char const*, int)
)
