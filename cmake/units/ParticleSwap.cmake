# The same sources as ParticleSave, built with TPW_SWAP_FILE_WORDS (CMakeLists.txt): the swap helpers of unit w168 really swap.
set(TPW_UNIT_ParticleSwap src/game/Particles.cpp src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_ParticleSwap_KEEP
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
