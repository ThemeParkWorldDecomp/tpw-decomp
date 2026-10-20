# Unit w199: a ride's wear, breakdowns, maintenance, repair and cleaning (the 18 CObject members of src/Object_upkeep.cpp). src/Object_queries.cpp supplies
# IsBrokenDown; every other callee (the ride database, IsRunning, IsCoaster, IsCoasterUseable, IsBOQConnected, GetFrontOfPhysicalQueue, SetState, the script
# variables, the sprite spawn, the hoardings, the world's advisor and bank, the map's regional effectors, the staff queries, the advisor request and its queue,
# the game type's constructor, the trace and the assertion) is a recording fake in tests/unit_ObjectUpkeep.cpp, which also defines the globals of other files.
set(TPW_UNIT_ObjectUpkeep src/Object_upkeep.cpp src/Object_queries.cpp platform/arena.cpp)
