set(TPW_UNIT_StartupSmall src/game/FixableTime.cpp src/game/VariableTime.cpp src/game/CommandLine.cpp
    src/game/Sound.cpp src/game/park/ParkSound.cpp src/game/MeshLoad.cpp src/game/Movie.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp src/game/ui/InterfaceFont.cpp src/game/ui/ui_main.cpp
    src/game/ExitErr.cpp)
set(TPW_UNIT_StartupSmall_KEEP
  __Z10MOVIE_Initv   # MOVIE_Init()
  __Z10MOVIE_PlayPKcP8GrafPortiii   # MOVIE_Play(char const*, GrafPort*, int, int, int)
  __Z10MOVIE_Stopv   # MOVIE_Stop()
  __Z10MOVIE_Tickv   # MOVIE_Tick()
  __Z11MOVIE_Closev   # MOVIE_Close()
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z14SND_DuckVolumei   # SND_DuckVolume(int)
  __Z15CheckFileExistsPc   # CheckFileExists(char*)
  __Z15MOVIE_SetVolumej   # MOVIE_SetVolume(unsigned int)
  __Z18SOUND_SetSfxStatusj   # SOUND_SetSfxStatus(unsigned int)
  __Z19UIMain_ConvertMultiPKh   # UIMain_ConvertMulti(unsigned char const*)
  __Z19UIMain_ConvertMultiPh   # UIMain_ConvertMulti(unsigned char*)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z20SOUND_SetMusicStatusj   # SOUND_SetMusicStatus(unsigned int)
  __Z20UIMain_ConvertStringPDs   # UIMain_ConvertString(char16_t*)
  __Z21SOUND_SetSpeechStatusj   # SOUND_SetSpeechStatus(unsigned int)
  __ZN10CExitError8SetErrorENS_9ErrorTypeEjPKc   # CExitError::SetError(CExitError::ErrorType, unsigned int, char const*)
  __ZN10CFixedTimeC1Ev   # CFixedTime::CFixedTime()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN12CCommandLine7GetArgcEv   # CCommandLine::GetArgc()
  __ZN12CCommandLine7GetArgvEv   # CCommandLine::GetArgv()
  __ZN12CCommandLineC1Ev   # CCommandLine::CCommandLine()
  __ZN12CCommandLineD1Ev   # CCommandLine::~CCommandLine()
  __ZN13CVariableTimeC1Ev   # CVariableTime::CVariableTime()
  __ZN13InterfaceFont7ReleaseEv   # InterfaceFont::Release()
  __ZN13InterfaceFont9GetHeightEv   # InterfaceFont::GetHeight()
  __ZN13InterfaceFontC1Ev   # InterfaceFont::InterfaceFont()
  __ZN13InterfaceFontD1Ev   # InterfaceFont::~InterfaceFont()
  __ZN20InterfaceFontWrapperC1EP7TbIFont   # InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*)
  __ZN20InterfaceFontWrapperD1Ev   # InterfaceFontWrapper::~InterfaceFontWrapper()
  __ZN8CoolRect14RectIntersectsEPS_   # CoolRect::RectIntersects(CoolRect*)
  __ZN8CoolRect3GetEPsS0_S0_S0_   # CoolRect::Get(short*, short*, short*, short*)
  __ZN8CoolRect3SetEssss   # CoolRect::Set(short, short, short, short)
  __ZN8CoolRect5ShiftEss   # CoolRect::Shift(short, short)
  __ZN8CoolRect6AdjustEssss   # CoolRect::Adjust(short, short, short, short)
  __ZN8CoolRect6MiddleEPsS0_   # CoolRect::Middle(short*, short*)
  __ZN8CoolRect7GetDimsEPsS0_   # CoolRect::GetDims(short*, short*)
  __ZN8CoolRect7PointInEss   # CoolRect::PointIn(short, short)
  __ZN8CoolRectC1Essss   # CoolRect::CoolRect(short, short, short, short)
  __ZN8CoolRectC1Ev   # CoolRect::CoolRect()
  __ZN8CoolRectD1Ev   # CoolRect::~CoolRect()
  _gAudioSystem
  _gExitError
  _gLobbyIslandSoundCount
  _gLobbyIslandSounds
  _gLobbyThunderBank
  _gLocalParkMusicBank
  _gMovie
  _gMovieVolume
  _gSoundAllocations
  _gSoundBanks
  _gSoundSettings
  _gSoundSystemOn
  _gSoundWanted
  _gUIHighPresets
  _gUIMainConvertBuffer
  _gUIMainMBBuffer
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
)
