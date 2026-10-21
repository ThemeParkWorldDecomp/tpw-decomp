set(TPW_UNIT_EngineStartup src/engine/RenderList.cpp src/engine/tb_resource_display.cpp src/engine/RenderMemory.cpp
    src/engine/Gamut.cpp src/engine/tb_hfield.cpp src/engine/LightManager.cpp src/engine/MapWho.cpp src/game/Light.cpp
    src/engine/SpriteManager.cpp src/engine/Switch.cpp src/engine/EngineUtil.cpp src/engine/Wavelet.cpp
    src/engine/tb_profiler.cpp src/engine/tb_depthoffield.cpp)
set(TPW_UNIT_EngineStartup_KEEP
  _GlobalDiffuse
  __Z10Gamut_Initv   # Gamut_Init()
  __Z11DefaultFreeRPv   # DefaultFree(void*&)
  __Z11Gamut_Closev   # Gamut_Close()
  __Z11HField_Initv   # HField_Init()
  __Z11MapWho_Initv   # MapWho_Init()
  __Z11ShiftFactorj   # ShiftFactor(unsigned int)
  __Z11Switch_Initv   # Switch_Init()
  __Z12FreeLinkBaseP12CMapLinkBase   # FreeLinkBase(CMapLinkBase*)
  __Z12HField_Closev   # HField_Close()
  __Z12InitWaveLoadv   # InitWaveLoad()
  __Z12MapWho_Closev   # MapWho_Close()
  __Z12Switch_Closev   # Switch_Close()
  __Z13CloseWaveLoadv   # CloseWaveLoad()
  __Z13DefaultMallocj   # DefaultMalloc(unsigned int)
  __Z13Particle_Initv   # Particle_Init()
  __Z14Particle_Closev   # Particle_Close()
  __Z15CreateSineTablev   # CreateSineTable()
  __Z15RenderList_Initv   # RenderList_Init()
  __Z16RegisterLinkBaseP12CMapLinkBase   # RegisterLinkBase(CMapLinkBase*)
  __Z16RenderList_Closev   # RenderList_Close()
  __Z16SetFileCallbacksPFPvPKcS1_EPFjS_EPFjS_jjS_EPFjS_jjEPFiS_EPFPcS1_iE   # SetFileCallbacks(void* (*)(char const*, char const*), unsigned int (*)(void*), unsigned int (*)(void*, unsigned int, unsigned int, void*), unsigned int (*)(void*, unsigned int, unsigned int), int (*)(void*), char* (*)(char const*, int))
  __Z17LightManager_Initv   # LightManager_Init()
  __Z17RenderMemory_Initv   # RenderMemory_Init()
  __Z18LightManager_Closev   # LightManager_Close()
  __Z18RenderMemory_Closev   # RenderMemory_Close()
  __Z18SetEngineCallbacksPFivEPFbvEPFjvEPFvvEPFPvjEPFvRS7_E   # SetEngineCallbacks(int (*)(), bool (*)(), unsigned int (*)(), void (*)(), void* (*)(unsigned int), void (*)(void*&))
  __Z18SpriteManager_Initv   # SpriteManager_Init()
  __Z19SpriteManager_Closev   # SpriteManager_Close()
  __Z7BitsSetj   # BitsSet(unsigned int)
  __ZN11CWaveletImp11GetRefCountEv   # CWaveletImp::GetRefCount()
  __ZN11CWaveletImp6AddRefEv   # CWaveletImp::AddRef()
  __ZN11CWaveletImp7ReleaseEv   # CWaveletImp::Release()
  __ZN11CWaveletImpC1Ev   # CWaveletImp::CWaveletImp()
  __ZN11CWaveletImpD1Ev   # CWaveletImp::~CWaveletImp()
  __ZN12CMapLinkBaseD1Ev   # CMapLinkBase::~CMapLinkBase()
  __ZN13CLightManagerC1Ev   # CLightManager::CLightManager()
  __ZN13CRenderMemory16LoadInfo_CopyVDBEi   # CRenderMemory::LoadInfo_CopyVDB(int)
  __ZN13CRenderMemory18GetVertexBufferPtrEi   # CRenderMemory::GetVertexBufferPtr(int)
  __ZN13CRenderMemory4InitEv   # CRenderMemory::Init()
  __ZN13CRenderMemory5CloseEv   # CRenderMemory::Close()
  __ZN13CRenderMemory5ResetEi   # CRenderMemory::Reset(int)
  __ZN13CRenderMemory8LoadInfoEi   # CRenderMemory::LoadInfo(int)
  __ZN13CRenderMemoryC1Ev   # CRenderMemory::CRenderMemory()
  __ZN14CSpriteManager5CloseEv   # CSpriteManager::Close()
  __ZN14CSpriteManagerC1Ev   # CSpriteManager::CSpriteManager()
  __ZN14CSpriteManagerD1Ev   # CSpriteManager::~CSpriteManager()
  __ZN6CGamut11SetYClipperEff   # CGamut::SetYClipper(float, float)
  __ZN6CGamutC1Ev   # CGamut::CGamut()
  __ZN7CHField10CloseGamutEv   # CHField::CloseGamut()
  __ZN7CHField5CloseEv   # CHField::Close()
  __ZN7CHFieldC1Ev   # CHField::CHField()
  __ZN7CHFieldD1Ev   # CHField::~CHField()
  __ZN7CMapWho16SetFinalCallbackEPFvvE   # CMapWho::SetFinalCallback(void (*)())
  __ZN7CMapWho5CloseEv   # CMapWho::Close()
  __ZN7CMapWhoC1Ev   # CMapWho::CMapWho()
  __ZN7CMapWhoD1Ev   # CMapWho::~CMapWho()
  __ZN8CDisplay10SetDisplayEiii   # CDisplay::SetDisplay(int, int, int)
  __ZN8CDisplay10SetMaxBitsEv   # CDisplay::SetMaxBits()
  __ZN8CDisplay11ResetWindowEv   # CDisplay::ResetWindow()
  __ZN8CDisplay11SetFogColorEj   # CDisplay::SetFogColor(unsigned int)
  __ZN8CDisplay11SetFogSpaceEff   # CDisplay::SetFogSpace(float, float)
  __ZN8CDisplay13SetBitPatternE8SBitDataS0_S0_S0_   # CDisplay::SetBitPattern(SBitData, SBitData, SBitData, SBitData)
  __ZN8CDisplay15OutputDebugInfoEv   # CDisplay::OutputDebugInfo()
  __ZN8CDisplay6ZSetupEfff   # CDisplay::ZSetup(float, float, float)
  __ZN8CDisplay9SetWindowEffff   # CDisplay::SetWindow(float, float, float, float)
  __ZN8CDisplay9SetZSpaceEf   # CDisplay::SetZSpace(float)
  __ZN8CDisplayC1Ev   # CDisplay::CDisplay()
  __ZN8CWavelet8InstanceEv   # CWavelet::Instance()
  __ZN8CWaveletD1Ev   # CWavelet::~CWavelet()
  __ZN9CParticle11GetLinkTypeEv   # CParticle::GetLinkType()
  __ZN9CParticle7GetDataEv   # CParticle::GetData()
  __ZN9CParticleC1Ev   # CParticle::CParticle()
  __ZN9CParticleD1Ev   # CParticle::~CParticle()
  _gFileClose
  _gFileName
  _gFileOpen
  _gFileRead
  _gFileSeek
  _gFileTell
  _gFree
  _gIsFirstPerson
  _gLinkBaseCount
  _gLinkBases
  _gMalloc
  _gReadClock
  _gSineTable
  _gUpdateMouse
  _gUseSmallTextures
  _gWaveLoadWavelet
  _gWaveletInstance
  _gWaveletWorkspace
  _gei_Display
  _gei_Gamut
  _gei_HField
  _gei_LightManager
  _gei_MapWho
  _gei_Particle
  _gei_RenderList
  _gei_RenderMemory
  _gei_SpriteManager
  _gei_Switch
  _gev_LightChain
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_EngineStartup_OWN_CALLLOG ON)
