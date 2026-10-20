# Unit w179: CCellBase (the neighbour, direction and overlap bookkeeping, the four map and track equivalents, Serialise and the LOADVAR /
# SAVEVAR<CELL_MODES> and LOADVARSTRUCT / SAVEVARSTRUCT<CELL_IndexType> instances) and CTrackCell (HasBeenModifiedFrom, Serialise, the two
# GetNeighbouringCell) with the real basic serialise instances (the file is the fake) and the eight compass deltas of src/MapDelta.cpp. Every
# other callee is a recording fake in tests/unit_CellBase.cpp.
set(TPW_UNIT_CellBase src/CellBase.cpp src/TrackCell.cpp src/Serialise_basic.cpp src/MapDelta.cpp)
set(TPW_UNIT_CellBase_OWN_CALLLOG ON)
