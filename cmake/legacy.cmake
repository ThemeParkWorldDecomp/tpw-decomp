# tpw_legacy (CMakeLists.txt) is the game library for the older main-app tests, whose fakes (TPW_LEGACY_STRING_FAKES)
# stand in for the bullfrog string classes and some file and time functions (those of the files TbString.cpp,
# TbStringList.cpp, TbStringBuffer.cpp, LbFile.cpp, LbTime.cpp and bfl_FileSystem.cpp of src/ before the regroup,
# which the library left out). It keeps the functions tpw_legacy_testshim took from it when every function had a
# file of its own, those of the files below of before the regroup (v5L30b): the library builds the src files that
# hold these (TPW_LEGACY_SOURCES) as copies that keep only them (TPW_LEGACY_KEEP, cmake/slicer.cmake).
# The files of before the regroup (src/ was flat then): Bitmap.cpp, CPU.cpp, Cheat.cpp, Clock.cpp, Colours.cpp,
# FS_Online.cpp, GlobalOrder.cpp, HLSprite.cpp, HoardingsBalance.cpp, LbExeKey.cpp, LbList.cpp, LbLog.cpp,
# MainBalance.cpp, MatrixPowerPC.cpp, MsgCheatMode.cpp, OnlineBalance.cpp, OptionsBalance.cpp,
# ResolutionBalance.cpp, SoundBalance.cpp, TPDialog.cpp, TPGlobals.cpp, TPGlobals_Dtor_TPGlobals.cpp, TPMain.cpp,
# TPOptions.cpp, TbFPUPrecision.cpp, TbPixFmt.cpp, TbRefRelease.cpp, TbTimeDiff.cpp, VanillaTime.cpp, Wavelet.cpp,
# bullfrog_CPU.cpp, fixmath.cpp, landscape.cpp, tb_capture.cpp, tb_error.cpp, tb_ezsprite.cpp, tb_light.cpp,
# tb_mapzoom.cpp, tb_matrix.cpp, tb_profiler.cpp, tb_rendermemory.cpp, tb_resource_display.cpp,
# tb_resource_system.cpp, tb_resource_texture.cpp, tb_shadow.cpp, tb_splash.cpp, vector2d.cpp.
set(TPW_LEGACY_SOURCES
  src/bullfrog/LbLog.cpp
  src/bullfrog/TbPixFmt.cpp
  src/bullfrog/TbRefRelease.cpp
  src/bullfrog/TbTimeDiff.cpp
  src/engine/Bitmap.cpp
  src/engine/EngineUtil.cpp
  src/engine/LightManager.cpp
  src/engine/M3D2Load.cpp
  src/engine/MapWho.cpp
  src/engine/MatrixPowerPC.cpp
  src/engine/RenderList.cpp
  src/engine/RenderMemory.cpp
  src/engine/Wavelet.cpp
  src/engine/landscape.cpp
  src/engine/tb_capture.cpp
  src/engine/tb_error.cpp
  src/engine/tb_ezsprite.cpp
  src/engine/tb_matrix.cpp
  src/engine/tb_profiler.cpp
  src/engine/tb_resource_display.cpp
  src/engine/tb_resource_system.cpp
  src/engine/tb_resource_texture.cpp
  src/engine/tb_shadow.cpp
  src/engine/tb_splash.cpp
  src/game/HLSprite.cpp
  src/game/Light.cpp
  src/game/TPGlobals.cpp
  src/game/TPMain.cpp
  src/game/TPOptions.cpp
  src/game/VanillaTime.cpp
  src/game/balance/HoardingsBalance.cpp
  src/game/balance/MainBalance.cpp
  src/game/balance/OnlineBalance.cpp
  src/game/balance/OptionsBalance.cpp
  src/game/balance/ResolutionBalance.cpp
  src/game/balance/SoundBalance.cpp
  src/game/online/FS_Online.cpp
  src/game/park/Clock.cpp
  src/game/park/GoldenTickets.cpp
  src/game/ui/Colours.cpp
  src/game/ui/TPDialog.cpp
  src/game/ui/vector2d.cpp
)
set(TPW_LEGACY_KEEP
  _GlobalDiffuse   # _GlobalDiffuse
  _TbPixFormat16   # _TbPixFormat16
  _TbPixFormat24   # _TbPixFormat24
  _TbPixFormat32   # _TbPixFormat32
  _TbPixFormat8   # _TbPixFormat8
  __Z10calc_coverPhS_S_S_S_   # calc_cover(unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*)
  __Z11DefaultFreeRPv   # DefaultFree(void*&)
  __Z11LbList_InitP6TbList   # LbList_Init(TbList*)
  __Z11LbLog_CloseP5TbLog   # LbLog_Close(TbLog*)
  __Z11LbLog_SetupP5TbLogPKcj   # LbLog_Setup(TbLog*, char const*, unsigned int)
  __Z11MapWho_Initv   # MapWho_Init()
  __Z11Matrix_Initv   # Matrix_Init()
  __Z11ShiftFactorj   # ShiftFactor(unsigned int)
  __Z12FreeLinkBaseP12CMapLinkBase   # FreeLinkBase(CMapLinkBase*)
  __Z12LbLog_DeleteP5TbLog   # LbLog_Delete(TbLog*)
  __Z12MapWho_Closev   # MapWho_Close()
  __Z12Matrix_Closev   # Matrix_Close()
  __Z12single_layerPhS_S_S_S_S_   # single_layer(unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*)
  __Z13DefaultMallocj   # DefaultMalloc(unsigned int)
  __Z13LbList_InsertP6TbListP10TbListNodeS2_   # LbList_Insert(TbList*, TbListNode*, TbListNode*)
  __Z13LbLog_RestartP5TbLog   # LbLog_Restart(TbLog*)
  __Z13LbLog_SuspendP5TbLog   # LbLog_Suspend(TbLog*)
  __Z14LbList_GetLastPK6TbList   # LbList_GetLast(TbList const*)
  __Z15CreateSineTablev   # CreateSineTable()
  __Z15LbList_GetFirstPK6TbList   # LbList_GetFirst(TbList const*)
  __Z15LbLog_SetPrefixP5TbLogPKc   # LbLog_SetPrefix(TbLog*, char const*)
  __Z15RenderList_Initv   # RenderList_Init()
  __Z16GetNumKidSpritesv   # GetNumKidSprites()
  __Z16RegisterLinkBaseP12CMapLinkBase   # RegisterLinkBase(CMapLinkBase*)
  __Z16RenderList_Closev   # RenderList_Close()
  __Z16SetFileCallbacksPFPvPKcS1_EPFjS_EPFjS_jjS_EPFjS_jjEPFiS_EPFPcS1_iE   # SetFileCallbacks(void* (*)(char const*, char const*), unsigned int (*)(void*), unsigned int (*)(void*, unsigned int, unsigned int, void*), unsigned int (*)(void*, unsigned int, unsigned int), int (*)(void*), char* (*)(char const*, int))
  __Z16resample_controlRK6BitmapjjjRS_   # resample_control(Bitmap const&, unsigned int, unsigned int, unsigned int, Bitmap&)
  __Z17GetOptionsSettingv   # GetOptionsSetting()
  __Z17LbList_NodeExistsPK6TbListPK10TbListNode   # LbList_NodeExists(TbList const*, TbListNode const*)
  __Z17LbLog_IsSuspendedP5TbLog   # LbLog_IsSuspended(TbLog*)
  __Z17LightManager_Initv   # LightManager_Init()
  __Z17RenderMemory_Initv   # RenderMemory_Init()
  __Z17RenderSystem_Initi   # RenderSystem_Init(int)
  __Z18LightManager_Closev   # LightManager_Close()
  __Z18PreprocessMatricesv   # PreprocessMatrices()
  __Z18RenderMemory_Closev   # RenderMemory_Close()
  __Z18RenderSystem_Closev   # RenderSystem_Close()
  __Z18RenderSystem_Flushv   # RenderSystem_Flush()
  __Z18RenderSystem_Resetiiii   # RenderSystem_Reset(int, int, int, int)
  __Z18SetEngineCallbacksPFivEPFbvEPFjvEPFvvEPFPvjEPFvRS7_E   # SetEngineCallbacks(int (*)(), bool (*)(), unsigned int (*)(), void (*)(), void* (*)(unsigned int), void (*)(void*&))
  __Z18ThemeParkWorldMainv   # ThemeParkWorldMain()
  __Z19LbExeKey_Initialisev   # LbExeKey_Initialise()
  __Z19LbLog_IsInitialisedP5TbLog   # LbLog_IsInitialised(TbLog*)
  __Z19RenderSystem_Renderv   # RenderSystem_Render()
  __Z20RenderSystem_MapLinkiiiiP12CMapLinkBasei   # RenderSystem_MapLink(int, int, int, int, CMapLinkBase*, int)
  __Z21RenderSystem_ResetAllv   # RenderSystem_ResetAll()
  __Z21line_circle_intersectR8Vector2dS0_S0_i   # line_circle_intersect(Vector2d&, Vector2d&, Vector2d&, int)
  __Z22RenderSystem_FrontLinkP12CMapLinkBasei   # RenderSystem_FrontLink(CMapLinkBase*, int)
  __Z22RenderSystem_MapUnLinkiiiiP12CMapLinkBase   # RenderSystem_MapUnLink(int, int, int, int, CMapLinkBase*)
  __Z22SetInitialOnlineValuesv   # SetInitialOnlineValues()
  __Z22xaxis_circle_intersectR8Vector2di   # xaxis_circle_intersect(Vector2d&, int)
  __Z23RenderSystem_FlipScreenv   # RenderSystem_FlipScreen()
  __Z23RenderSystem_FrontReseti   # RenderSystem_FrontReset(int)
  __Z24ConvertSurfaceSizeToFlagi   # ConvertSurfaceSizeToFlag(int)
  __Z24LbExeKey_ReferenceNumberv   # LbExeKey_ReferenceNumber()
  __Z24RenderSystem_FrontUnlinkP12CMapLinkBasei   # RenderSystem_FrontUnlink(CMapLinkBase*, int)
  __Z26RenderSystem_ClearViewportv   # RenderSystem_ClearViewport()
  __Z5LbLogP5TbLogPKcz   # LbLog(TbLog*, char const*, ...)
  __Z5isqrtj   # isqrt(unsigned int)
  __Z6FixDivii   # FixDiv(int, int)
  __Z6FixMulii   # FixMul(int, int)
  __Z6lubksbR7sMatrixPiPf   # lubksb(sMatrix&, int*, float*)
  __Z6ludcmpR7sMatrixPiPf   # ludcmp(sMatrix&, int*, float*)
  __Z7BitsSetj   # BitsSet(unsigned int)
  __Z7blend_2PhS_S_S_S_S_S_S_S_S_S_   # blend_2(unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*)
  __Z7blend_3PhS_S_S_S_S_S_S_S_S_S_S_S_S_S_S_   # blend_3(unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*, unsigned char*)
  __ZN10TbMaskInfo3SetEjj   # TbMaskInfo::Set(unsigned int, unsigned int)
  __ZN10TbTimeDiff3SetEiiiii   # TbTimeDiff::Set(int, int, int, int, int)
  __ZN10TbTimeDiffdVEf   # TbTimeDiff::operator/=(float)
  __ZN10TbTimeDiffdVEi   # TbTimeDiff::operator/=(int)
  __ZN10TbTimeDiffmIERKS_   # TbTimeDiff::operator-=(TbTimeDiff const&)
  __ZN10TbTimeDiffmLEf   # TbTimeDiff::operator*=(float)
  __ZN10TbTimeDiffmLEi   # TbTimeDiff::operator*=(int)
  __ZN10TbTimeDiffpLERKS_   # TbTimeDiff::operator+=(TbTimeDiff const&)
  __ZN11CWaveletImp11GetRefCountEv   # CWaveletImp::GetRefCount()
  __ZN11CWaveletImp6AddRefEv   # CWaveletImp::AddRef()
  __ZN11CWaveletImp7ReleaseEv   # CWaveletImp::Release()
  __ZN11CWaveletImpC1Ev   # CWaveletImp::CWaveletImp()
  __ZN11CWaveletImpD1Ev   # CWaveletImp::~CWaveletImp()
  __ZN11TbPixelCaps3SetERK11TbPixFormat   # TbPixelCaps::Set(TbPixFormat const&)
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN12CMapLinkBaseD1Ev   # CMapLinkBase::~CMapLinkBase()
  __ZN12CVanillaTime4LoadEPv   # CVanillaTime::Load(void*)
  __ZN12CVanillaTime4SaveEPv   # CVanillaTime::Save(void*)
  __ZN12OnlineConfigC1Ev   # OnlineConfig::OnlineConfig()
  __ZN12TbRefRelease6AddRefEv   # TbRefRelease::AddRef()
  __ZN12TbRefRelease7ReleaseEv   # TbRefRelease::Release()
  __ZN12TbRefReleaseC1Ev   # TbRefRelease::TbRefRelease()
  __ZN12TbRefReleaseD1Ev   # TbRefRelease::~TbRefRelease()
  __ZN13CErrorHandler15GetAltTabStatusEv   # CErrorHandler::GetAltTabStatus()
  __ZN13CErrorHandler3PopEv   # CErrorHandler::Pop()
  __ZN13CErrorHandler4PushEii   # CErrorHandler::Push(int, int)
  __ZN13CLightManagerC1Ev   # CLightManager::CLightManager()
  __ZN13CMsgCheatModeD1Ev   # CMsgCheatMode::~CMsgCheatMode()
  __ZN13CRenderMemory16LoadInfo_CopyVDBEi   # CRenderMemory::LoadInfo_CopyVDB(int)
  __ZN13CRenderMemory18GetVertexBufferPtrEi   # CRenderMemory::GetVertexBufferPtr(int)
  __ZN13CRenderMemory4InitEv   # CRenderMemory::Init()
  __ZN13CRenderMemory5CloseEv   # CRenderMemory::Close()
  __ZN13CRenderMemory5ResetEi   # CRenderMemory::Reset(int)
  __ZN13CRenderMemory8LoadInfoEi   # CRenderMemory::LoadInfo(int)
  __ZN13CRenderMemoryC1Ev   # CRenderMemory::CRenderMemory()
  __ZN13CSoundBalance21GetBalanceDescriptionEi   # CSoundBalance::GetBalanceDescription(int)
  __ZN13CSoundBalance21GetBalanceValuesStartEv   # CSoundBalance::GetBalanceValuesStart()
  __ZN13CSoundBalanceD1Ev   # CSoundBalance::~CSoundBalance()
  __ZN14CMatrixPowerPC10MultMatrixEPKfS1_Pf   # CMatrixPowerPC::MultMatrix(float const*, float const*, float*)
  __ZN14CMatrixPowerPC11TransformUVEP16STMCoordinateSetP6sUVSetiii   # CMatrixPowerPC::TransformUV(STMCoordinateSet*, sUVSet*, int, int, int)
  __ZN14CMatrixPowerPC5LightEP11SFVF_TLCTEXP9tag_sNodeP8sFaceSeti   # CMatrixPowerPC::Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int)
  __ZN14CMatrixPowerPC7ProjectEPviii   # CMatrixPowerPC::Project(void*, int, int, int)
  __ZN14CMatrixPowerPC9TransformEP7sMatrixPvP14sVertexPosnSetiii   # CMatrixPowerPC::Transform(sMatrix*, void*, sVertexPosnSet*, int, int, int)
  __ZN14CMatrixPowerPC9TransformEP7sMatrixPvS2_iiii   # CMatrixPowerPC::Transform(sMatrix*, void*, void*, int, int, int, int)
  __ZN14CMatrixPowerPCD1Ev   # CMatrixPowerPC::~CMatrixPowerPC()
  __ZN14COnlineBalance21GetBalanceDescriptionEi   # COnlineBalance::GetBalanceDescription(int)
  __ZN14COnlineBalance21GetBalanceValuesStartEv   # COnlineBalance::GetBalanceValuesStart()
  __ZN14COnlineBalanceD1Ev   # COnlineBalance::~COnlineBalance()
  __ZN14CTransformBase13TestMathSpeedEv   # CTransformBase::TestMathSpeed()
  __ZN14CTransformBaseD1Ev   # CTransformBase::~CTransformBase()
  __ZN14ThemeParkWorld10InitialiseEv   # ThemeParkWorld::Initialise()
  __ZN14ThemeParkWorld11IsCdPresentEv   # ThemeParkWorld::IsCdPresent()
  __ZN14ThemeParkWorld12HandleTypingEccjb   # ThemeParkWorld::HandleTyping(char, char, unsigned int, bool)
  __ZN14ThemeParkWorld13GetUIMousePosERKN13SamsUtilities6UEventERsS4_   # ThemeParkWorld::GetUIMousePos(SamsUtilities::UEvent const&, short&, short&)
  __ZN14ThemeParkWorld13HandleCommandEij   # ThemeParkWorld::HandleCommand(int, unsigned int)
  __ZN14ThemeParkWorld13HandleMouseUpERKN13SamsUtilities6UEventENS0_11MouseButtonE   # ThemeParkWorld::HandleMouseUp(SamsUtilities::UEvent const&, SamsUtilities::MouseButton)
  __ZN14ThemeParkWorld15HandleMouseDownERKN13SamsUtilities6UEventENS0_11MouseButtonE   # ThemeParkWorld::HandleMouseDown(SamsUtilities::UEvent const&, SamsUtilities::MouseButton)
  __ZN14ThemeParkWorld16HandleMouseMovedERKN13SamsUtilities6UEventE   # ThemeParkWorld::HandleMouseMoved(SamsUtilities::UEvent const&)
  __ZN14ThemeParkWorld16HandleMouseWheelERKN13SamsUtilities6UEventEi   # ThemeParkWorld::HandleMouseWheel(SamsUtilities::UEvent const&, int)
  __ZN14ThemeParkWorld18HandleUpdateWindowEP8GrafPort   # ThemeParkWorld::HandleUpdateWindow(GrafPort*)
  __ZN14ThemeParkWorld20HandleActivateWindowEP8GrafPortb   # ThemeParkWorld::HandleActivateWindow(GrafPort*, bool)
  __ZN14ThemeParkWorld22LoadSystemBalanceFilesEv   # ThemeParkWorld::LoadSystemBalanceFiles()
  __ZN14ThemeParkWorld7ProcessEv   # ThemeParkWorld::Process()
  __ZN14ThemeParkWorld8CdDialog6FilterERKN13SamsUtilities6UEventERs   # ThemeParkWorld::CdDialog::Filter(SamsUtilities::UEvent const&, short&)
  __ZN14ThemeParkWorld8ShutdownEv   # ThemeParkWorld::Shutdown()
  __ZN14ThemeParkWorld9LookForCdEv   # ThemeParkWorld::LookForCd()
  __ZN14ThemeParkWorldD1Ev   # ThemeParkWorld::~ThemeParkWorld()
  __ZN15CFSOnlineSystem14StopProcessingEv   # CFSOnlineSystem::StopProcessing()
  __ZN15CFSOnlineSystem4TickEv   # CFSOnlineSystem::Tick()
  __ZN15COptionsBalance21GetBalanceDescriptionEi   # COptionsBalance::GetBalanceDescription(int)
  __ZN15COptionsBalance21GetBalanceValuesStartEv   # COptionsBalance::GetBalanceValuesStart()
  __ZN15COptionsBalanceD1Ev   # COptionsBalance::~COptionsBalance()
  __ZN16CTextureSettings12GetAlphaMaskEv   # CTextureSettings::GetAlphaMask()
  __ZN16CTextureSettings15OutputDebugInfoEv   # CTextureSettings::OutputDebugInfo()
  __ZN16CTextureSettings16CalculateUVFixupEv   # CTextureSettings::CalculateUVFixup()
  __ZN16CTextureSettingsC1Ev   # CTextureSettings::CTextureSettings()
  __ZN17CDrawToBackBuffer5CloseEv   # CDrawToBackBuffer::Close()
  __ZN17CDrawToBackBuffer7OverlayEPcii   # CDrawToBackBuffer::Overlay(char*, int, int)
  __ZN17CDrawToBackBufferD1Ev   # CDrawToBackBuffer::~CDrawToBackBuffer()
  __ZN17CHoardingsBalance21GetBalanceDescriptionEi   # CHoardingsBalance::GetBalanceDescription(int)
  __ZN17CHoardingsBalance21GetBalanceValuesStartEv   # CHoardingsBalance::GetBalanceValuesStart()
  __ZN17CHoardingsBalanceD1Ev   # CHoardingsBalance::~CHoardingsBalance()
  __ZN18CResolutionBalance21GetBalanceDescriptionEi   # CResolutionBalance::GetBalanceDescription(int)
  __ZN18CResolutionBalance21GetBalanceValuesStartEv   # CResolutionBalance::GetBalanceValuesStart()
  __ZN18CResolutionBalanceD1Ev   # CResolutionBalance::~CResolutionBalance()
  __ZN3CPU19clocks_per_millisecEi   # CPU::clocks_per_millisec(int)
  __ZN3CPU6detectEv   # CPU::detect()
  __ZN6Bitmap11colour_fillEhhhh   # Bitmap::colour_fill(unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap11shadowblendERS_   # Bitmap::shadowblend(Bitmap&)
  __ZN6Bitmap12load_waveletEPvP8CWavelet   # Bitmap::load_wavelet(void*, CWavelet*)
  __ZN6Bitmap17swizzle_for_gimexEv   # Bitmap::swizzle_for_gimex()
  __ZN6Bitmap4loadEPv   # Bitmap::load(void*)
  __ZN6Bitmap5clearEv   # Bitmap::clear()
  __ZN6Bitmap7copy_toERS_   # Bitmap::copy_to(Bitmap&)
  __ZN6Bitmap7reallocEjjj   # Bitmap::realloc(unsigned int, unsigned int, unsigned int)
  __ZN6Bitmap8alphabltERS_jj   # Bitmap::alphablt(Bitmap&, unsigned int, unsigned int)
  __ZN6Bitmap9colourbltERS_hhhh   # Bitmap::colourblt(Bitmap&, unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap9descimateERS_   # Bitmap::descimate(Bitmap&)
  __ZN6CCheat10IsCheatingEv   # CCheat::IsCheating()
  __ZN6CCheat14SetCheatStatusEb   # CCheat::SetCheatStatus(bool)
  __ZN6CCheat4LoadERPv   # CCheat::Load(void*&)
  __ZN6CCheatC1Ev   # CCheat::CCheat()
  __ZN6CClock11TogglePauseEv   # CClock::TogglePause()
  __ZN6CClock12GetPauseFlagEv   # CClock::GetPauseFlag()
  __ZN6CClock4LoadEPv   # CClock::Load(void*)
  __ZN6CClock4ReadEv   # CClock::Read()
  __ZN6CClock4SaveEPv   # CClock::Save(void*)
  __ZN6CClock5PauseEv   # CClock::Pause()
  __ZN6CClock7PreSaveEv   # CClock::PreSave()
  __ZN6CClock7UnPauseEv   # CClock::UnPause()
  __ZN6CClock8PostLoadEv   # CClock::PostLoad()
  __ZN6CClock9ReadFixedEv   # CClock::ReadFixed()
  __ZN7CMapWho16SetFinalCallbackEPFvvE   # CMapWho::SetFinalCallback(void (*)())
  __ZN7CMapWho5CloseEv   # CMapWho::Close()
  __ZN7CMapWhoC1Ev   # CMapWho::CMapWho()
  __ZN7CMapWhoD1Ev   # CMapWho::~CMapWho()
  __ZN7CMatrix10ProjectionEfff   # CMatrix::Projection(float, float, float)
  __ZN7CMatrix15TransformVertexEPvS0_i   # CMatrix::TransformVertex(void*, void*, int)
  __ZN7CMatrix16AdjustProjectionEf   # CMatrix::AdjustProjection(float)
  __ZN7CMatrix4LoadER7sMatrix   # CMatrix::Load(sMatrix&)
  __ZN7CMatrix4ViewE10_D3DVECTORS0_S0_f   # CMatrix::View(_D3DVECTOR, _D3DVECTOR, _D3DVECTOR, float)
  __ZN7CMatrix7InverseEv   # CMatrix::Inverse()
  __ZN7CMatrix7RotateXEf   # CMatrix::RotateX(float)
  __ZN7CMatrix7RotateYEf   # CMatrix::RotateY(float)
  __ZN7CMatrix7RotateZEf   # CMatrix::RotateZ(float)
  __ZN7CShadow14AddLightSourceE10_D3DVECTOR   # CShadow::AddLightSource(_D3DVECTOR)
  __ZN7CShadow17ResetLightSourcesEv   # CShadow::ResetLightSources()
  __ZN7CShadow19GetNextShadowMatrixEv   # CShadow::GetNextShadowMatrix()
  __ZN7CShadow20GetFirstShadowMatrixEv   # CShadow::GetFirstShadowMatrix()
  __ZN7CShadowD1Ev   # CShadow::~CShadow()
  __ZN7CSplash14WinProgressBarEv   # CSplash::WinProgressBar()
  __ZN7CSplash17InterpolateSplashEv   # CSplash::InterpolateSplash()
  __ZN7CSplash4InitEPcS0_i   # CSplash::Init(char*, char*, int)
  __ZN7CSplash5CloseEv   # CSplash::Close()
  __ZN7CSplashD1Ev   # CSplash::~CSplash()
  __ZN7CSystem12UpdateLightsEv   # CSystem::UpdateLights()
  __ZN7CSystem15OutputDebugInfoEv   # CSystem::OutputDebugInfo()
  __ZN7CSystemC1Ev   # CSystem::CSystem()
  __ZN7Tileset4loadEPv   # Tileset::load(void*)
  __ZN7Tileset5clearEv   # Tileset::clear()
  __ZN7TilesetC1Ev   # Tileset::Tileset()
  __ZN7TilesetD1Ev   # Tileset::~Tileset()
  __ZN7sMatrix8ByteswapEv   # sMatrix::Byteswap()
  __ZN8CCapture10GetCaptureEv   # CCapture::GetCapture()
  __ZN8CCapture10SetCaptureEPc   # CCapture::SetCapture(char*)
  __ZN8CCapture14ConvertCaptureEv   # CCapture::ConvertCapture()
  __ZN8CCapture19SaveUncompressedTGAEPc   # CCapture::SaveUncompressedTGA(char*)
  __ZN8CCapture7CaptureEv   # CCapture::Capture()
  __ZN8CColours9CalibrateEv   # CColours::Calibrate()
  __ZN8CColoursC1Ev   # CColours::CColours()
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
  __ZN8COptionsD1Ev   # COptions::~COptions()
  __ZN8CWavelet8InstanceEv   # CWavelet::Instance()
  __ZN8CWaveletD1Ev   # CWavelet::~CWavelet()
  __ZN8bullfrog12gCPUClockMHzE   # bullfrog::gCPUClockMHz
  __ZN8bullfrog3CPU19clocks_per_millisecEi   # bullfrog::CPU::clocks_per_millisec(int)
  __ZN8bullfrog3CPU6detectEv   # bullfrog::CPU::detect()
  __ZN9CEZSprite10LoadSpriteEPc   # CEZSprite::LoadSprite(char*)
  __ZN9CEZSprite12DeleteSpriteEPv   # CEZSprite::DeleteSprite(void*)
  __ZN9CEZSprite16GetSpriteDetailsEPv   # CEZSprite::GetSpriteDetails(void*)
  __ZN9CEZSprite17SetSpritePositionEPv10_D3DVECTOR   # CEZSprite::SetSpritePosition(void*, _D3DVECTOR)
  __ZN9CEZSprite20CreateSpriteInstanceEPv   # CEZSprite::CreateSpriteInstance(void*)
  __ZN9CEZSprite20DeleteSpriteInstanceEPv   # CEZSprite::DeleteSpriteInstance(void*)
  __ZN9CEZSprite4InitEii   # CEZSprite::Init(int, int)
  __ZN9CEZSprite5CloseEv   # CEZSprite::Close()
  __ZN9CEZSprite6EnableEv   # CEZSprite::Enable()
  __ZN9CEZSprite7DisableEv   # CEZSprite::Disable()
  __ZN9CEZSpriteC1Ev   # CEZSprite::CEZSprite()
  __ZN9CEZSpriteD1Ev   # CEZSprite::~CEZSprite()
  __ZN9CProfiler21GetClockCycleCount10hEv   # CProfiler::GetClockCycleCount10h()
  __ZN9Landscape12set_tilesizeEi   # Landscape::set_tilesize(int)
  __ZN9Landscape13generate_tileEPhjjP12sTileOverlay   # Landscape::generate_tile(unsigned char*, unsigned int, unsigned int, sTileOverlay*)
  __ZN9Landscape14load_landscapeEPc   # Landscape::load_landscape(char*)
  __ZN9Landscape14precalc_tablesEv   # Landscape::precalc_tables()
  __ZN9Landscape5clearEv   # Landscape::clear()
  __ZN9LandscapeC1Ev   # Landscape::Landscape()
  __ZN9LandscapeD1Ev   # Landscape::~Landscape()
  __ZN9TPGlobalsC1Ev   # TPGlobals::TPGlobals()
  __ZN9TPGlobalsD1Ev   # TPGlobals::~TPGlobals()
  __ZN9TbFPUFlag10CheckFlagsEv   # TbFPUFlag::CheckFlags()
  __ZN9TbFPUFlag5ResetEv   # TbFPUFlag::Reset()
  __ZN9TbFPUFlag8SetFlagsEtt   # TbFPUFlag::SetFlags(unsigned short, unsigned short)
  __ZN9TbFPUFlag9DoFlagGetEv   # TbFPUFlag::DoFlagGet()
  __ZN9TbFPUFlag9DoFlagSetEv   # TbFPUFlag::DoFlagSet()
  __ZNK10TbTimeDiff3GetEPiS0_S0_S0_S0_   # TbTimeDiff::Get(int*, int*, int*, int*, int*) const
  __ZNK10TbTimeDiffeqERKS_   # TbTimeDiff::operator==(TbTimeDiff const&) const
  __ZNK10TbTimeDiffgeERKS_   # TbTimeDiff::operator>=(TbTimeDiff const&) const
  __ZNK10TbTimeDiffgtERKS_   # TbTimeDiff::operator>(TbTimeDiff const&) const
  __ZNK10TbTimeDiffleERKS_   # TbTimeDiff::operator<=(TbTimeDiff const&) const
  __ZNK10TbTimeDiffltERKS_   # TbTimeDiff::operator<(TbTimeDiff const&) const
  __ZNK10TbTimeDiffmiERKS_   # TbTimeDiff::operator-(TbTimeDiff const&) const
  __ZNK10TbTimeDiffneERKS_   # TbTimeDiff::operator!=(TbTimeDiff const&) const
  __ZNK10TbTimeDiffplERK11TbTimeStamp   # TbTimeDiff::operator+(TbTimeStamp const&) const
  __ZNK10TbTimeDiffplERKS_   # TbTimeDiff::operator+(TbTimeDiff const&) const
  __ZNK13CMsgCheatMode7GetTypeEv   # CMsgCheatMode::GetType() const
  __ZNK6CCheat4SaveERPv   # CCheat::Save(void*&) const
  __ZNK8COptions16GetNumKidSpritesEv   # COptions::GetNumKidSprites() const
  __ZNK8COptions18GetNumStaffSpritesEv   # COptions::GetNumStaffSprites() const
  _gAppName   # _gAppName
  _gCPUClockMHz   # _gCPUClockMHz
  _gClock   # _gClock
  _gColours   # _gColours
  _gControl1   # _gControl1
  _gControl2   # _gControl2
  _gControl3   # _gControl3
  _gFileClose   # _gFileClose
  _gFileName   # _gFileName
  _gFileOpen   # _gFileOpen
  _gFileRead   # _gFileRead
  _gFileSeek   # _gFileSeek
  _gFileTell   # _gFileTell
  _gFree   # _gFree
  _gGame   # _gGame
  _gGameOptions   # _gGameOptions
  _gHLSpriteLists   # _gHLSpriteLists
  _gHoardingsBalanceDescriptions   # _gHoardingsBalanceDescriptions
  _gIsFirstPerson   # _gIsFirstPerson
  _gLbExeKey   # _gLbExeKey
  _gLbExeKeyInitialised   # _gLbExeKeyInitialised
  _gLbExeKeyState   # _gLbExeKeyState
  _gLightInverseWorld   # _gLightInverseWorld
  _gLinkBaseCount   # _gLinkBaseCount
  _gLinkBases   # _gLinkBases
  _gMainBalance   # _gMainBalance
  _gMainBalanceDescriptions   # _gMainBalanceDescriptions
  _gMainWindow   # _gMainWindow
  _gMalloc   # _gMalloc
  _gNoSavedSettings   # _gNoSavedSettings
  _gOnlineBalance   # _gOnlineBalance
  _gOnlineBalanceDescriptions   # _gOnlineBalanceDescriptions
  _gOnlineConfig   # _gOnlineConfig
  _gOptions   # _gOptions
  _gOptionsBalance   # _gOptionsBalance
  _gOptionsBalanceDescriptions   # _gOptionsBalanceDescriptions
  _gProfiler   # _gProfiler
  _gProjectionFOV   # _gProjectionFOV
  _gProjectionFar   # _gProjectionFar
  _gProjectionNear   # _gProjectionNear
  _gRampA   # _gRampA
  _gRampB   # _gRampB
  _gRampC   # _gRampC
  _gRampCover1   # _gRampCover1
  _gRampCover2   # _gRampCover2
  _gReadClock   # _gReadClock
  _gResolutionBalance   # _gResolutionBalance
  _gResolutionBalanceDescriptions   # _gResolutionBalanceDescriptions
  _gScreenCentreX   # _gScreenCentreX
  _gScreenCentreY   # _gScreenCentreY
  _gScreenHeight   # _gScreenHeight
  _gScreenMatrix   # _gScreenMatrix
  _gScreenWidth   # _gScreenWidth
  _gShadow   # _gShadow
  _gShadowHeadingReference   # _gShadowHeadingReference
  _gSineTable   # _gSineTable
  _gSoundBalance   # _gSoundBalance
  _gSoundBalanceDescriptions   # _gSoundBalanceDescriptions
  _gSplashFirst   # _gSplashFirst
  _gSplashStartMs   # _gSplashStartMs
  _gTPDialogLanguage   # _gTPDialogLanguage
  _gTPDialogLanguageBits   # _gTPDialogLanguageBits
  _gTPDialogLanguages   # _gTPDialogLanguages
  _gTPDialogOptions   # _gTPDialogOptions
  _gTPDialogTextureMegabytes   # _gTPDialogTextureMegabytes
  _gTPDialogTitle   # _gTPDialogTitle
  _gTPGlobals   # _gTPGlobals
  _gTPMainLanguage   # _gTPMainLanguage
  _gTPMainLanguages   # _gTPMainLanguages
  _gTPMainTitle   # _gTPMainTitle
  _gTPMainUpload   # _gTPMainUpload
  _gTPMain_102b2ba4   # _gTPMain_102b2ba4
  _gTPMain_102b2bac   # _gTPMain_102b2bac
  _gTPWFlags   # _gTPWFlags
  _gThemeParkWorld   # _gThemeParkWorld
  _gTileBuffer   # _gTileBuffer
  _gUpVector   # _gUpVector
  _gUpdateMouse   # _gUpdateMouse
  _gUseSmallTextures   # _gUseSmallTextures
  _gVanillaTime   # _gVanillaTime
  _gViewAngle   # _gViewAngle
  _gViewCentreX   # _gViewCentreX
  _gViewCentreY   # _gViewCentreY
  _gViewDir2D   # _gViewDir2D
  _gViewDir2DPtr   # _gViewDir2DPtr
  _gWaveletInstance   # _gWaveletInstance
  _gWaveletWorkspace   # _gWaveletWorkspace
  _g_100b00fc   # _g_100b00fc
  _g_vector2d_xaxis   # _g_vector2d_xaxis
  _gei_Capture   # _gei_Capture
  _gei_Display   # _gei_Display
  _gei_EZSprite   # _gei_EZSprite
  _gei_ErrorHandler   # _gei_ErrorHandler
  _gei_FinalMatrix   # _gei_FinalMatrix
  _gei_InternalFinalMatrix   # _gei_InternalFinalMatrix
  _gei_InternalInvViewMatrix   # _gei_InternalInvViewMatrix
  _gei_InternalViewMatrix   # _gei_InternalViewMatrix
  _gei_InvViewMatrix   # _gei_InvViewMatrix
  _gei_LightManager   # _gei_LightManager
  _gei_LightMatrix   # _gei_LightMatrix
  _gei_MapWho   # _gei_MapWho
  _gei_MathUnit   # _gei_MathUnit
  _gei_ProjectionMatrix   # _gei_ProjectionMatrix
  _gei_RenderList   # _gei_RenderList
  _gei_RenderMemory   # _gei_RenderMemory
  _gei_Splash   # _gei_Splash
  _gei_System   # _gei_System
  _gei_TextureSettings   # _gei_TextureSettings
  _gei_ViewMatrix   # _gei_ViewMatrix
  _gev_LightChain   # _gev_LightChain
)
