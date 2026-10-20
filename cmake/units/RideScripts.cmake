# The ride scripts (unit w132): RSSE_Tick, RSSE_RenderTick, RSSE_Getvar, RSSE_GetScriptPointer, RSSE_SetReverbOnEax, SCR_ScriptTick,
# SCR_CloseScriptSystem, SCR_BuildScriptSpriteList and CObject::UpdateTurn, ModelState, CreateRideScript, UpdateEngine. The arena is the allocator
# behind the Ptr32 links of the script records and the script table; gMainBalance is the real storage. Every callee outside the set (the
# interpreter, the object and walk node ticks, the kill, the setters, the ride database, the ride instancing, the sprites, the thing id and the
# trace) is a fake in the unit file.
set(TPW_UNIT_RideScripts src/rsse_ride_tick.cpp src/ScriptTick.cpp src/Object_turn.cpp src/MainBalance.cpp src/CFixedTime.cpp src/CVariableTime.cpp platform/arena.cpp)
