# Unit w206: the park's inside filled from the locked hoarding and a hoarding edit checked for legality and cost (src/LLCellSystem_hoardedit.cpp) with
# the cell flag and kind tests (src/TrackCell_isflag.cpp, src/MapCell_kinds.cpp). The flag and kind tests of unit w201 (src/TrackCell_flags.cpp,
# src/MapCell_flags.cpp), the track cell's straight test (src/TrackCell_shape.cpp), the cell bases and the track cell's neighbouring cell (src/CellBase.cpp,
# src/TrackCell.cpp) and the eight compass deltas (src/MapDelta.cpp) run for real; every other callee is a recording fake in tests/unit_HoardingEdit.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_HoardingEdit src/LLCellSystem_hoardedit.cpp src/TrackCell_isflag.cpp src/MapCell_kinds.cpp src/TrackCell_flags.cpp src/MapCell_flags.cpp src/TrackCell_shape.cpp src/CellBase.cpp src/TrackCell.cpp src/MapDelta.cpp src/Serialise_basic.cpp)
