set(TPW_UNIT_ParticlesInit src/bullfrog/LbZLib.cpp src/game/Particles.cpp src/game/rides/rsse.cpp
    src/game/rides/RidePlacement.cpp src/game/ui/InterfaceFont.cpp src/game/ui/InterfaceStringHeap.cpp
    src/ltms/TbFontRenderParams.cpp src/bullfrog/LbSemaphore.cpp)
set(TPW_UNIT_ParticlesInit_KEEP
  __Z10PAR_Malloci   # PAR_Malloc(int)
  __Z10SwapHeaderP14ZLibCompHeader   # SwapHeader(ZLibCompHeader*)
  __Z12PAR_SetupSinv   # PAR_SetupSin()
  __Z14PAR_SetLowSpeci   # PAR_SetLowSpec(int)
  __Z15PAR_LoadLibraryPc   # PAR_LoadLibrary(char*)
  __Z18SEC_AuthorizedUserv   # SEC_AuthorizedUser()
  __Z19DummyHeightCallBackii   # DummyHeightCallBack(int, int)
  __Z20LbZLib_MemDecompressPvS_S_   # LbZLib_MemDecompress(void*, void*, void*)
  __Z21PAR_LoadLibraryResultPc   # PAR_LoadLibraryResult(char*)
  __Z22PAR_InitParticleSystemPvii   # PAR_InitParticleSystem(void*, int, int)
  __Z22PAR_SwapLibraryEmitterR17PARLibraryEmitter   # PAR_SwapLibraryEmitter(PARLibraryEmitter&)
  __Z23PAR_ResetParticleSystemv   # PAR_ResetParticleSystem()
  __Z26SetThemeTextureDirectoriesi   # SetThemeTextureDirectories(int)
  __ZN11_LbSemaLock4LockEi   # _LbSemaLock::Lock(int)
  __ZN11_LbSemaLockD1Ev   # _LbSemaLock::~_LbSemaLock()
  __ZN13InterfaceFont15Gen8x8DebugFontEsssss   # InterfaceFont::Gen8x8DebugFont(short, short, short, short, short)
  __ZN13SamsUtilities12USwapBlock32EPii   # SamsUtilities::USwapBlock32(int*, int)
  __ZN13SamsUtilities14USwapInplace16ERs   # SamsUtilities::USwapInplace16(short&)
  __ZN13SamsUtilities14USwapInplace16ERt   # SamsUtilities::USwapInplace16(unsigned short&)
  __ZN13SamsUtilities14USwapInplace32ERi   # SamsUtilities::USwapInplace32(int&)
  __ZN18TbFontRenderParams17UseAlphaWriteModeEv   # TbFontRenderParams::UseAlphaWriteMode()
  __ZN18TbFontRenderParamsC1ERKS_   # TbFontRenderParams::TbFontRenderParams(TbFontRenderParams const&)
  __ZN18TbFontRenderParamsC1Ev   # TbFontRenderParams::TbFontRenderParams()
  __ZN18TbFontRenderParamsD1Ev   # TbFontRenderParams::~TbFontRenderParams()
  __ZN19InterfaceStringHeap10DeAllocateEv   # InterfaceStringHeap::DeAllocate()
  __ZN19InterfaceStringHeap13ReleaseStringEPDs   # InterfaceStringHeap::ReleaseString(char16_t*)
  __ZN19InterfaceStringHeap14AllocateResultEiiii   # InterfaceStringHeap::AllocateResult(int, int, int, int)
  __ZN19InterfaceStringHeap8AllocateEiiii   # InterfaceStringHeap::Allocate(int, int, int, int)
  __ZNK3bfl8compress5CZLib13MemDecompressEPhS2_S2_   # bfl::compress::CZLib::MemDecompress(unsigned char*, unsigned char*, unsigned char*) const
  __ZNK3bfl8compress5CZLib21CompressWorkspaceSizeEv   # bfl::compress::CZLib::CompressWorkspaceSize() const
  _gAuthorizationStamp
  _gAuthorizedUser
  _gParDrawingList
  _gParDrawingListBusy
  _gParEmitters
  _gParLibrary
  _gParLibraryWordA
  _gParLibraryWordB
  _gParSinTable
  _gParState
  _gParSystemRunning
  _gSecurityKey
  _gSecurityMask
  _gSecurityText
  _gSharedTextureFolder
  _gStringHeapRowsA
  _gStringHeapRowsB
  _gStringHeapTableA
  _gStringHeapTableB
  _gStringHeapWidthA
  _gStringHeapWidthB
  _gThemeTextureFolder
)
