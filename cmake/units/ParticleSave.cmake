# The particle system save and load (unit w168): the real state block and the allocator arena; every callee is a fake in the unit file.
set(TPW_UNIT_ParticleSave src/game/Particles.cpp src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_ParticleSave_KEEP
  __Z14PAR_LoadStatusPv   # PAR_LoadStatus(void*)
  __Z14PAR_SaveStatusPvi   # PAR_SaveStatus(void*, int)
  __ZN11tagPARTICLE8ByteswapEv   # tagPARTICLE::Byteswap()
  __ZN13SamsUtilities14USwap32LongPtrEj   # SamsUtilities::USwap32LongPtr(unsigned int)
  __ZN13SamsUtilities14USwap32VoidPtrEj   # SamsUtilities::USwap32VoidPtr(unsigned int)
  __ZN13SamsUtilities16USwapBlockStructEP17PARLibraryEmitterj   # SamsUtilities::USwapBlockStruct(PARLibraryEmitter*, unsigned int)
  __ZN13SamsUtilities16USwapBlockStructEP18PARLibraryEffecterj   # SamsUtilities::USwapBlockStruct(PARLibraryEffecter*, unsigned int)
  __ZN13SamsUtilities18USwap32ParticlePtrEj   # SamsUtilities::USwap32ParticlePtr(unsigned int)
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
