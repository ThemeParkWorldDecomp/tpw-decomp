# Unit w169: the life of a ride script: RSSE_InitScript reads a script file into a new record, KillScript gives a record and its tables and
# objects back, RSSE_KillScript starts the closing effect and kills the linked scripts (src/rsse_script_life.cpp). The arena is the allocator
# behind the record and its tables (the Ptr32 links). The callees outside the unit (the file reads and the byte swap, the ride database, the
# particle and sound calls, the data nodes, the script lookup, the trace) are fakes in tests/unit_RideScriptLife.cpp, which also defines the
# globals of other files.
set(TPW_UNIT_RideScriptLife src/rsse_script_life.cpp platform/arena.cpp)
