# Unit w204: the click stacks of the queue and track tools (backtracking, the queue walk, the two track walks), the track shape tests and the
# track end offsets: src/LLCellSystem_clickstacks.cpp, src/TrackCell_shape.cpp, src/MapCell_corner.cpp, src/LLCellRide_offsets.cpp. The cell base,
# the map delta and track cell files run for real (GetNeighbours, NumberOfNeighbours, RemoveNeighbour, SetOverlapCounter, OPPOSITE, ADD_DIRECTION,
# IsXYValid, the two GetNeighbouringCell, CTrackCell::IsCorner); src/MapReset.cpp is only here for CCellBase::RemoveNeighbour. Every other callee is
# a recording fake in tests/unit_ClickStacks.cpp (what MapReset.cpp and TrackCell.cpp drag along is a no-op there, never called).
set(TPW_UNIT_ClickStacks src/LLCellSystem_clickstacks.cpp src/TrackCell_shape.cpp src/MapCell_corner.cpp src/LLCellRide_offsets.cpp
    src/CellBase.cpp src/MapReset.cpp src/MapDelta_map.cpp src/TrackCell.cpp src/TrackCell_flags.cpp platform/arena.cpp)
