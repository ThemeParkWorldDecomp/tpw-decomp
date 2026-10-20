# Unit w163: the script interpreter's core (AssignSlot, InitScript, DoScript, the stack, the spawn and the setters, ESP_InitScript,
# ESP_SpawnScript and ten of the PK_ commands; src/ScriptInterp.cpp). The arena is the allocator behind the Ptr32 words of the script table.
# The check table, the code array, the globals of the script system, the clock read, the sprites and the 32 other PK_ commands are
# fakes or storage in tests/unit_ScriptInterp.cpp; src/ScriptBlocks.cpp is not linked (its pair is faked there).
set(TPW_UNIT_ScriptInterp src/ScriptInterp.cpp src/ScriptHostSprites.cpp src/CFixedTime.cpp src/CVariableTime.cpp platform/arena.cpp)
