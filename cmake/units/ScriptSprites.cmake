# Unit w165: the script sprites (ESP_ForceGoto, ESP_RURunningThis, ESP_Spawn2DCopyOfScriptAt, ESP_Update2DScript,
# ESP_FixSpriteIDsFromDetail and thirteen PK_ commands; src/ScriptSprites.cpp) and the script save and load (SCR_SaveAllScripts,
# SCR_LoadAllScripts and the file swaps; src/ScriptSave.cpp). The arena is the allocator behind the Ptr32 words of the script table.
# The code array, the table's globals, the allocator, the file reads and writes, the swaps of platform/sams_core.cpp, the sprites and
# the other callees are fakes or storage in tests/unit_ScriptSprites.cpp; src/ScriptInterp.cpp and src/ScriptBlocks.cpp are not linked.
set(TPW_UNIT_ScriptSprites src/ScriptSprites.cpp src/ScriptSave.cpp platform/arena.cpp)
