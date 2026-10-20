# Unit w185: the cell system's modes, ride words, costs, cursor, click stack and first block fix up (src/LLCellSystem_modes.cpp). The cost words and
# the click stack count come from the two small landed files; the landed src/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real. Every other
# callee is a recording fake in tests/unit_LLCellModes.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCellModes src/LLCellSystem_modes.cpp src/LLCellSystem_costs.cpp src/LLCellClicks.cpp src/Mouse.cpp platform/arena.cpp)
