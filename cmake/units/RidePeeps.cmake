# Unit w149: the peep tables of a ride script (WALKNODE_Tick, Trigger, Get, Float, Release; BOUNCE_Put, Get, ForceGet, Tick; LIMBO_Put, Get,
# ForceGet; GetNodePos; src/game/rides/RideEffects.cpp). The callees outside the unit (the peep drawing, the embedded-node lookups, the clock, the ride heads,
# the emitter, the assertion and the peep's sprite) are recording fakes in tests/unit_RidePeeps.cpp, which also defines the globals of other files.
set(TPW_UNIT_RidePeeps src/game/rides/RideEffects.cpp platform/arena.cpp)
set(TPW_UNIT_RidePeeps_KEEP
  __Z10BOUNCE_GetP11RSSE_SCRIPT   # BOUNCE_Get(RSSE_SCRIPT*)
  __Z10BOUNCE_PutP11RSSE_SCRIPTji   # BOUNCE_Put(RSSE_SCRIPT*, unsigned int, int)
  __Z10GetNodePosP11RSSE_SCRIPTP9JCPOINT3DS2_ii   # GetNodePos(RSSE_SCRIPT*, JCPOINT3D*, JCPOINT3D*, int, int)
  __Z11BOUNCE_TickP11RSSE_SCRIPT   # BOUNCE_Tick(RSSE_SCRIPT*)
  __Z12WALKNODE_GetP11RSSE_SCRIPT   # WALKNODE_Get(RSSE_SCRIPT*)
  __Z13WALKNODE_TickP11RSSE_SCRIPT   # WALKNODE_Tick(RSSE_SCRIPT*)
  __Z14LIMBO_ForceGetP11RSSE_SCRIPT   # LIMBO_ForceGet(RSSE_SCRIPT*)
  __Z14WALKNODE_FloatP11RSSE_SCRIPTiii   # WALKNODE_Float(RSSE_SCRIPT*, int, int, int)
  __Z15BOUNCE_ForceGetP11RSSE_SCRIPT   # BOUNCE_ForceGet(RSSE_SCRIPT*)
  __Z16WALKNODE_ReleaseP11RSSE_SCRIPTj   # WALKNODE_Release(RSSE_SCRIPT*, unsigned int)
  __Z16WALKNODE_TriggerP11RSSE_SCRIPTjiiiiii   # WALKNODE_Trigger(RSSE_SCRIPT*, unsigned int, int, int, int, int, int, int)
  __Z9LIMBO_GetP11RSSE_SCRIPT   # LIMBO_Get(RSSE_SCRIPT*)
  __Z9LIMBO_PutP11RSSE_SCRIPTji   # LIMBO_Put(RSSE_SCRIPT*, unsigned int, int)
)
