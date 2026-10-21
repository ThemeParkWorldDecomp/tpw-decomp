# Unit v5342: COnlineControl::SaveBuddy and ::ProcessVoteResponse (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_SaveBuddy.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_SaveBuddy src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_SaveBuddy_KEEP
  __Z15OnlineSaveBuddyP14COnlineControlPKc   # OnlineSaveBuddy(COnlineControl*, char const*)
  __ZN14COnlineControl19ProcessVoteResponseEi   # COnlineControl::ProcessVoteResponse(int)
  __ZN14COnlineControl9SaveBuddyEPKc   # COnlineControl::SaveBuddy(char const*)
)
