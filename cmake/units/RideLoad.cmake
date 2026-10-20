# Unit w167: RSSE_Load (the ride scripts' load) and RSSE_WALK::Byteswap (src/rsse_save.cpp, with w166's RSSE_Save and the swap helpers). The arena is
# the allocator behind the Ptr32 links of the loaded records, tables and objects. The file read, the memory trace, the asserts, the script kill, the
# system's init, the word swap, the ride bound area and the sound call are fakes in tests/unit_RideLoad.cpp, which also defines the globals of
# other files and the ride sets.
set(TPW_UNIT_RideLoad src/rsse_save.cpp platform/arena.cpp)
