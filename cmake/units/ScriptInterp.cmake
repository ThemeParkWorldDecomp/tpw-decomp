# Unit w163: the script interpreter's core (AssignSlot, InitScript, DoScript, the stack, the spawn and the setters, ESP_InitScript,
# ESP_SpawnScript and ten of the PK_ commands; src/game/Script.cpp, src/game/ScriptOps.cpp). The arena is the allocator behind the Ptr32 words of the script table.
# The check table, the code array, the globals of the script system, the clock read, the sprites and the 32 other PK_ commands are
# fakes or storage in tests/unit_ScriptInterp.cpp; src/game/park/analyser.cpp is not linked (its pair is faked there).
set(TPW_UNIT_ScriptInterp src/game/Script.cpp src/game/ScriptOps.cpp src/game/park/analyser.cpp
    src/game/FixableTime.cpp src/game/VariableTime.cpp platform/arena.cpp)
set(TPW_UNIT_ScriptInterp_KEEP
  __Z10AssignSlotP9tagSCRIPT   # AssignSlot(tagSCRIPT*)
  __Z10InitScriptP9tagSCRIPTPvifff   # InitScript(tagSCRIPT*, void*, int, float, float, float)
  __Z10PK_PUSHVARv   # PK_PUSHVAR()
  __Z10PK_SETTEXTv   # PK_SETTEXT()
  __Z12SCR_CodeWordj   # SCR_CodeWord(unsigned int)
  __Z14ESP_InitScriptP9tagSCRIPTPv11ESP_Spritesi4ACTSfffii   # ESP_InitScript(tagSCRIPT*, void*, ESP_Sprites, int, ACTS, float, float, float, int, int)
  __Z15ESP_SpawnScriptPv11ESP_Spritesi4ACTSfff   # ESP_SpawnScript(void*, ESP_Sprites, int, ACTS, float, float, float)
  __Z15SCR_CodeAddressPKv   # SCR_CodeAddress(void const*)
  __Z15SCR_SetVariableiji   # SCR_SetVariable(int, unsigned int, int)
  __Z15SCR_SpawnScriptPvifff   # SCR_SpawnScript(void*, int, float, float, float)
  __Z20ESP_ScriptHostSpritePK9tagSCRIPT   # ESP_ScriptHostSprite(tagSCRIPT const*)
  __Z20SCR_SetFloatVariableijf   # SCR_SetFloatVariable(int, unsigned int, float)
  __Z21SCR_GetScriptPositioniPfS_S_Pi   # SCR_GetScriptPosition(int, float*, float*, float*, int*)
  __Z21SCR_SetScriptPositionifffi   # SCR_SetScriptPosition(int, float, float, float, int)
  __Z22ESP_IsScriptHostSpritePK9tagSCRIPTPKv   # ESP_IsScriptHostSprite(tagSCRIPT const*, void const*)
  __Z22SCR_SetServiceIntervalii   # SCR_SetServiceInterval(int, int)
  __Z28SCR_WhichScriptHasThisSpritePv   # SCR_WhichScriptHasThisSprite(void*)
  __Z6PK_ENDv   # PK_END()
  __Z6PK_NOPv   # PK_NOP()
  __Z7PK_GOTOv   # PK_GOTO()
  __Z7PK_WAITv   # PK_WAIT()
  __Z7SCR_Popv   # SCR_Pop()
  __Z8DoScriptP9tagSCRIPT   # DoScript(tagSCRIPT*)
  __Z8PK_GOSUBv   # PK_GOSUB()
  __Z8SCR_Pushi   # SCR_Push(int)
  __Z9PK_POPVARv   # PK_POPVAR()
  __Z9PK_RETURNv   # PK_RETURN()
  __Z9PK_SETVARv   # PK_SETVAR()
  __Z9SCR_ErrorPKc   # SCR_Error(char const*)
  __ZN10CFixedTimeC1Ev   # CFixedTime::CFixedTime()
  __ZN13CVariableTimeC1Ev   # CVariableTime::CVariableTime()
  _gScriptCodeBase
  _gScriptCurrent
  _gScriptStartAddresses
  _gScriptVars
)
