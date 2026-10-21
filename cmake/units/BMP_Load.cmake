# Unit v5L09: the advisor load, BMP_Load (src/game/rides/BumperRide.cpp), with the chunk reader of unit v51017 (Read_OpenChunk, Read_DataFromChunk,
# Read_BumpFromChunk; src/game/rides/BumperRide.cpp). The other callees are recording fakes in tests/unit_BMP_Load.cpp.
set(TPW_UNIT_BMP_Load src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_Load_KEEP
  __Z14Read_OpenChunkPvP5Ptr32I11tabBMPCHUNKE   # Read_OpenChunk(void*, Ptr32<tabBMPCHUNK>*)
  __Z18Read_BumpFromChunkP11tabBMPCHUNKR7tagBUMP   # Read_BumpFromChunk(tabBMPCHUNK*, tagBUMP&)
  __Z18Read_DataFromChunkP11tabBMPCHUNKPvj   # Read_DataFromChunk(tabBMPCHUNK*, void*, unsigned int)
  __Z8BMP_LoadPv   # BMP_Load(void*)
)
