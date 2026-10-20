# Unit w198: a ride's takings, costs, prizes and satisfaction (src/Object_takings.cpp; 29 CObject members). The history sums and averages
# (src/ParkAnalyser_history.cpp) and the world accessor (src/WorldSavvy.cpp) are real; every other callee (the ride database, the world's accessors, the bank
# account, the challenge counter, the map's effectors, the script calls, the random number, the game type constructor, the funny time and its difference,
# the string table, the assertion and the trace) is a recording fake in tests/unit_ObjectTakings.cpp, which also defines the globals of other files.
set(TPW_UNIT_ObjectTakings src/Object_takings.cpp src/ParkAnalyser_history.cpp src/WorldSavvy.cpp platform/arena.cpp)
