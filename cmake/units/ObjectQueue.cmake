# Unit w197: the ride's queue (src/Object_queue.cpp). The callees outside the unit are fakes in tests/unit_ObjectQueue.cpp (the assert, the trace, the ride script
# variables, the world accessors and its random generator, the thing table's persons, the ride predicates, the wall test, CPerson::EvaluateQueueEdit and
# InQueueingState); the cell code (CellBase, MapDelta, MapDelta_map: GetNeighbouringCell, OPPOSITE, the time stamps) runs as translated. Serialise_basic.cpp
# supplies the LOADVAR/SAVEVAR instances those files link against; the symbols nothing calls (the map cells' mesh refresh, the save log, the file calls) are
# link fakes at the end of the unit's test file.
set(TPW_UNIT_ObjectQueue src/Object_queue.cpp src/CellBase.cpp src/MapDelta_map.cpp src/MapDelta.cpp src/Serialise_basic.cpp platform/arena.cpp)
