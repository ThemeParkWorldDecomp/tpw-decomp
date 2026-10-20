# Unit w172: a ride script's ride-specific commands (src/rsse_commands.cpp) and the script program reader they call (src/rsse_objects.cpp: ReadScript,
# ExtractData). The ride callees (FLY_, BMP_, Coaster, LLCC_, GetCoasterInstanceID, FindDataNodeIndexByID) are fakes in tests/unit_RideCommands.cpp, which also
# defines the globals of other files. The arena is the allocator behind the script record and its Ptr32 links.
set(TPW_UNIT_RideCommands src/rsse_commands.cpp src/rsse_objects.cpp platform/arena.cpp)
