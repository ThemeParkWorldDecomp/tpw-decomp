# Unit v5998: LLCR_StartQueue and LLCR_StartTrack (src/game/park/LLCellRide.cpp). The callees are recording fakes in
# tests/unit_LLCR_StartQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCR_StartQueue src/game/park/LLCellRide.cpp platform/arena.cpp)
set(TPW_UNIT_LLCR_StartQueue_KEEP
  __Z15LLCR_StartQueueP8LLCR_PRRPiS1_   # LLCR_StartQueue(LLCR_PRR*, int*, int*)
  __Z15LLCR_StartTrackP8LLCR_PRRPiS1_i   # LLCR_StartTrack(LLCR_PRR*, int*, int*, int)
)
