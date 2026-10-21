# Unit w26a: the particle system's core, second half (built with TPW_SWAP_FILE_WORDS below, like unit ParticlesInit).
set(TPW_UNIT_Particles src/game/Particles.cpp src/game/rides/rsse.cpp)
set(TPW_UNIT_Particles_KEEP
  __Z10PAR_Malloci   # PAR_Malloc(int)
  __Z12PAR_SetupSinv   # PAR_SetupSin()
  __Z14PAR_SetLowSpeci   # PAR_SetLowSpec(int)
  __Z14PAR_TickResultv   # PAR_TickResult()
  __Z15PAR_KillEmitteri   # PAR_KillEmitter(int)
  __Z15PAR_LoadLibraryPc   # PAR_LoadLibrary(char*)
  __Z17PAR_SetEmitterPosiiii   # PAR_SetEmitterPos(int, int, int, int)
  __Z18PAR_KillLibEmitterj   # PAR_KillLibEmitter(unsigned int)
  __Z18PAR_LoadLibEmitterPN3bfl10FileSystem5CFileE   # PAR_LoadLibEmitter(bfl::FileSystem::CFile*)
  __Z18PAR_SetEmitterLifeii   # PAR_SetEmitterLife(int, int)
  __Z18PAR_SetEmitterMaskii   # PAR_SetEmitterMask(int, int)
  __Z19DummyHeightCallBackii   # DummyHeightCallBack(int, int)
  __Z19PAR_FreeDrawingListP4PTCL   # PAR_FreeDrawingList(PTCL*)
  __Z19PAR_SetEmitterScaleii   # PAR_SetEmitterScale(int, int)
  __Z20PAR_GetEmitterStatusj   # PAR_GetEmitterStatus(unsigned int)
  __Z21PAR_GetEffecterStatusj   # PAR_GetEffecterStatus(unsigned int)
  __Z21PAR_LoadLibraryResultPc   # PAR_LoadLibraryResult(char*)
  __Z22PAR_InitParticleSystemPvii   # PAR_InitParticleSystem(void*, int, int)
  __Z22PAR_SwapLibraryEmitterR17PARLibraryEmitter   # PAR_SwapLibraryEmitter(PARLibraryEmitter&)
  __Z23PAR_CloseParticleSystemv   # PAR_CloseParticleSystem()
  __Z23PAR_ResetParticleSystemv   # PAR_ResetParticleSystem()
  __Z23PAR_SetEmitterRandomPosiiii   # PAR_SetEmitterRandomPos(int, int, int, int)
  __Z25PAR_GetEmitterBoundingBoxjP10PARPOINT3DS0_   # PAR_GetEmitterBoundingBox(unsigned int, PARPOINT3D*, PARPOINT3D*)
  __Z26PAR_SetEmitterLaunchVectoriiii   # PAR_SetEmitterLaunchVector(int, int, int, int)
  __Z27PAR_KillAndHideAll2DObjectsv   # PAR_KillAndHideAll2DObjects()
  __Z30ApplyParticleDensityAdjustmenti   # ApplyParticleDensityAdjustment(int)
  __Z8PAR_FreePv   # PAR_Free(void*)
  __Z8PAR_Tickv   # PAR_Tick()
  __ZN13SamsUtilities12USwapBlock32EPii   # SamsUtilities::USwapBlock32(int*, int)
  __ZN13SamsUtilities14USwapInplace16ERs   # SamsUtilities::USwapInplace16(short&)
  __ZN13SamsUtilities14USwapInplace16ERt   # SamsUtilities::USwapInplace16(unsigned short&)
  __ZN13SamsUtilities14USwapInplace32ERi   # SamsUtilities::USwapInplace32(int&)
  _gParAllocBalance
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
