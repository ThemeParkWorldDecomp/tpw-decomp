# Unit v5247: the chunk writer (Write_BumpToChunk, Write_CloseChunk; src/game/rides/BumperRide.cpp). The callees outside the unit are
# recording fakes in tests/unit_Write_BumpToChunk.cpp.
set(TPW_UNIT_Write_BumpToChunk src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_Write_BumpToChunk_KEEP
  __Z16Write_CloseChunkP11tabBMPCHUNK   # Write_CloseChunk(tabBMPCHUNK*)
  __Z17Write_BumpToChunkP11tabBMPCHUNKR7tagBUMP   # Write_BumpToChunk(tabBMPCHUNK*, tagBUMP&)
)
