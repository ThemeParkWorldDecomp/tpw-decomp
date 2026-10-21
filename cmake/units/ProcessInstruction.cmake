# Unit v5882: ProcessInstruction 0x100cfdf4, a ride script's instruction interpreter (src/game/rides/rsse.cpp). The callees
# outside the unit are recording fakes in tests/unit_ProcessInstruction.cpp, which also defines the globals of other files.
set(TPW_UNIT_ProcessInstruction src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_ProcessInstruction_KEEP
  __Z18ProcessInstructionP11RSSE_SCRIPT   # ProcessInstruction(RSSE_SCRIPT*)
)
