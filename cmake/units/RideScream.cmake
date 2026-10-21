# Unit w171: a ride script's screams, the repair particle effect, DrawPeep and two getters of rsse.cpp (src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp). The callees outside
# the unit (the sound events, the emitters, GetNodePos, the ride database, the peep's sprite, the clock and the assertion) are recording fakes in
# tests/unit_RideScream.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideScream src/game/rides/RideEffects.cpp src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_RideScream_KEEP
  __Z11SCREAM_Moveiiii   # SCREAM_Move(int, int, int, int)
  __Z11SCREAM_Stopi   # SCREAM_Stop(int)
  __Z12SCREAM_Startiiiiiii   # SCREAM_Start(int, int, int, int, int, int, int)
  __Z13SCREAM_Singleiiiiii   # SCREAM_Single(int, int, int, int, int, int)
  __Z15SCREAM_SetLeveliii   # SCREAM_SetLevel(int, int, int)
  __Z16SCREAM_SingleSadiiii   # SCREAM_SingleSad(int, int, int, int)
  __Z21RSSE_GetCoasterHandlei   # RSSE_GetCoasterHandle(int)
  __Z22RSSE_GetSoundEventCodeii   # RSSE_GetSoundEventCode(int, int)
  __Z27TriggerRepairParticleEffectP11RSSE_SCRIPTi   # TriggerRepairParticleEffect(RSSE_SCRIPT*, int)
  __Z8DrawPeepP11RSSE_SCRIPTP9RSSE_WALKiiii   # DrawPeep(RSSE_SCRIPT*, RSSE_WALK*, int, int, int, int)
  _gRepairEffectGuard
  _gRepairEffectWord
)
