# The ride scripts (unit w132): RSSE_Tick, RSSE_RenderTick, RSSE_Getvar, RSSE_GetScriptPointer, RSSE_SetReverbOnEax, SCR_ScriptTick,
# SCR_CloseScriptSystem, SCR_BuildScriptSpriteList and CObject::UpdateTurn, ModelState, CreateRideScript, UpdateEngine. The arena is the allocator
# behind the Ptr32 links of the script records and the script table; gMainBalance is the real storage. Every callee outside the set (the
# interpreter, the object and walk node ticks, the kill, the setters, the ride database, the ride instancing, the sprites, the thing id and the
# trace) is a fake in the unit file.
set(TPW_UNIT_RideScripts src/game/rides/rsse.cpp src/game/Script.cpp src/game/Object.cpp
    src/game/balance/MainBalance.cpp src/game/FixableTime.cpp src/game/VariableTime.cpp platform/arena.cpp)
set(TPW_UNIT_RideScripts_KEEP
  __Z11RSSE_GetvarP11RSSE_SCRIPTi   # RSSE_Getvar(RSSE_SCRIPT*, int)
  __Z14SCR_ScriptTickv   # SCR_ScriptTick()
  __Z15RSSE_RenderTickv   # RSSE_RenderTick()
  __Z19RSSE_SetReverbOnEaxv   # RSSE_SetReverbOnEax()
  __Z21RSSE_GetScriptPointeri   # RSSE_GetScriptPointer(int)
  __Z21SCR_CloseScriptSystemv   # SCR_CloseScriptSystem()
  __Z25SCR_BuildScriptSpriteListv   # SCR_BuildScriptSpriteList()
  __Z9RSSE_Tickv   # RSSE_Tick()
  __ZN10CFixedTimeC1Ev   # CFixedTime::CFixedTime()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CVariableTimeC1Ev   # CVariableTime::CVariableTime()
  __ZN7CObject10ModelStateEv   # CObject::ModelState()
  __ZN7CObject10UpdateTurnEv   # CObject::UpdateTurn()
  __ZN7CObject12UpdateEngineEi   # CObject::UpdateEngine(int)
  __ZN7CObject16CreateRideScriptEv   # CObject::CreateRideScript()
  _gMainBalance
  _gMainBalanceDescriptions
  _gScriptTickTime
  _gScriptTickTotal
)
