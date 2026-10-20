# Unit w146: the map save and litter half of MapDelta.cpp (CMapCell::PlaceLitter, the CMap, CMapCell and CMacroAI serialisers, the LitterType
# instances) with the real basic serialise instances (the file is the fake) and the three cell constructors (src/CellInit.cpp, run real on both
# sides) and src/MapDelta.cpp (the eight compass deltas, whose values the test compares with __sinit_MapDelta_cpp's). Every other callee is a recording fake in tests/unit_MapSave.cpp, which includes calllog.cpp itself (the map walk logs many events).
set(TPW_UNIT_MapSave src/MapDelta_save.cpp src/Serialise_basic.cpp src/CellInit.cpp src/MapDelta.cpp)
set(TPW_UNIT_MapSave_OWN_CALLLOG ON)
