# Unit v51017: the advisor save's chunk reader (Read_OpenChunk, Read_DataFromChunk, Read_BumpFromChunk; src/game/rides/BumperRide.cpp).
# The callees outside the unit are recording fakes in tests/unit_Read_BumpFromChunk.cpp.
set(TPW_UNIT_Read_BumpFromChunk src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_Read_BumpFromChunk_KEEP
  __Z14Read_OpenChunkPvP5Ptr32I11tabBMPCHUNKE   # Read_OpenChunk(void*, Ptr32<tabBMPCHUNK>*)
  __Z18Read_BumpFromChunkP11tabBMPCHUNKR7tagBUMP   # Read_BumpFromChunk(tabBMPCHUNK*, tagBUMP&)
  __Z18Read_DataFromChunkP11tabBMPCHUNKPvj   # Read_DataFromChunk(tabBMPCHUNK*, void*, unsigned int)
)
