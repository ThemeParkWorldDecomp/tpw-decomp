# Unit v5638: COnlineControl::ProcessResponse. The callees outside the unit are recording fakes in
# tests/unit_COnlineControl_ProcessResponse.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_ProcessResponse src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_ProcessResponse_KEEP
  __ZN14COnlineControl15ProcessResponseEPv   # COnlineControl::ProcessResponse(void*)
)
