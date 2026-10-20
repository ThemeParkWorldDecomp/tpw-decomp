# Unit w166: RSSE_Save (the ride scripts' save) and rsse.cpp's eleven swap helpers (src/rsse_save.cpp). The arena is the allocator behind the
# Ptr32 links of the script records. The state, the file write, the SamsUtilities copy / allocate / delete and the thing id's swap are
# fakes or storage in tests/unit_RideSave.cpp.
set(TPW_UNIT_RideSave src/rsse_save.cpp platform/arena.cpp)
