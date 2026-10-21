# Unit v5248: COnlineControl::ProcessInitResponse (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_ProcessInitResponse.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_ProcessInitResponse src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_ProcessInitResponse_KEEP
  __ZN14COnlineControl19ProcessInitResponseEPv   # COnlineControl::ProcessInitResponse(void*)
)
