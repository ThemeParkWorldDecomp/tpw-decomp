# Unit w164: the script interpreter's flow and arithmetic (SCR_FindNextEndif, SCR_FindNextElseOrEndif, PK_IF, ENDIF, ELSE, REPEAT, UNTIL, DO,
# LOOP and the twelve arithmetic commands; src/ScriptFlow.cpp). The script system's globals, the code array, SCR_Error, SCR_Push, SCR_Pop and
# CWorld::GetRand are fakes or storage in tests/unit_ScriptFlow.cpp; src/ScriptInterp.cpp is not linked.
set(TPW_UNIT_ScriptFlow src/ScriptFlow.cpp platform/arena.cpp)
