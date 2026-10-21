# Unit v5988: LLCC_TraceTrack (src/game/park/LLCellRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCC_TraceTrack.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_TraceTrack src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_TraceTrack_KEEP
  __Z15LLCC_TraceTrackP10CTrackCellS0_i   # LLCC_TraceTrack(CTrackCell*, CTrackCell*, int)
  __Z20LLCC_TraceTrackCountP10CTrackCellS0_i   # LLCC_TraceTrackCount(CTrackCell*, CTrackCell*, int)
)
