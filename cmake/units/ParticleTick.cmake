# The particle system's tick and drawing list and the effect manager (unit w26c). The state block is the real one; the callees in the
# spawning and core units are fakes in the unit file.
set(TPW_UNIT_ParticleTick src/game/Particles.cpp src/game/HLSprite.cpp src/game/rides/rsse.cpp)
set(TPW_UNIT_ParticleTick_KEEP
  __Z10DoEmittersv   # DoEmitters()
  __Z11DoAParticlei   # DoAParticle(int)
  __Z11DoEffectersv   # DoEffecters()
  __Z11MoveEmitterP10PAREmitter   # MoveEmitter(PAREmitter*)
  __Z12MoveEffecterP11PAREffecter   # MoveEffecter(PAREffecter*)
  __Z15EM_SetEmitersOniii   # EM_SetEmitersOn(int, int, int)
  __Z17EM_SetEffectOrbitiifff   # EM_SetEffectOrbit(int, int, float, float, float)
  __Z20PAR_BuildDrawingListi   # PAR_BuildDrawingList(int)
  __Z23EM_SetEffectTranslationiifff   # EM_SetEffectTranslation(int, int, float, float, float)
  __Z7EM_Initv   # EM_Init()
  __Z7EM_Tickv   # EM_Tick()
  _gEffects
  _gParDrawingList
  _gParDrawingListBusy
  _gParEmitters
  _gParLibrary
  _gParLibraryWordA
  _gParLibraryWordB
  _gParSinTable
  _gParState
  _gParSystemRunning
)
