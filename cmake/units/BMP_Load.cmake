# Unit v5L09: the advisor load, BMP_Load (src/BMP_Load.cpp), with the chunk reader of unit v51017 (Read_OpenChunk, Read_DataFromChunk,
# Read_BumpFromChunk; src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp). The other callees are recording fakes in tests/unit_BMP_Load.cpp.
set(TPW_UNIT_BMP_Load src/BMP_Load.cpp src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp platform/arena.cpp)
