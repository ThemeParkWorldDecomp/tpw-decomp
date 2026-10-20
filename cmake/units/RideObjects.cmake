# Unit w150: the script objects of the ride scripts (emitters and sounds), the script stack and program reader, the small script getters and
# two ride position getters (src/rsse_objects.cpp). The arena is the allocator behind the objects and the Ptr32 links of the script record. The
# callees outside the unit (the node position, the particle and sound calls, the ride database, the matrix helpers, the trace) are fakes in
# tests/unit_RideObjects.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideObjects src/rsse_objects.cpp platform/arena.cpp)
