# The sound runtime (unit w66): SOUND_Init, SOUND_Close, SOUND_Tick, SOUND_SendEvent and the other wrappers over the abstract audio
# object, with the state in TPSound.cpp. Everything the runtime calls outside the unit (CreateAudioObject, the sams keyboard,
# GAME_ASSERTMSG) is a fake in the unit file; gSoundBalance is raw storage there.
# The game builds TbSysCommand objects whose Execute bodies are the library's (unit w79h): tests/fake_syscommands.cpp answers 0 for them.
set(TPW_UNIT_SoundRuntime src/game/Sound.cpp src/game/park/ParkSound.cpp tests/fake_syscommands.cpp)
set(TPW_UNIT_SoundRuntime_KEEP
  __Z10SOUND_InitPcji   # SOUND_Init(char*, unsigned int, int)
  __Z10SOUND_Tickv   # SOUND_Tick()
  __Z11SOUND_Closev   # SOUND_Close()
  __Z11SOUND_Pausev   # SOUND_Pause()
  __Z13SOUND_UnPausev   # SOUND_UnPause()
  __Z14SND_DuckVolumei   # SND_DuckVolume(int)
  __Z15SOUND_AddObjectfffff   # SOUND_AddObject(float, float, float, float, float)
  __Z15SOUND_DelObjectj   # SOUND_DelObject(unsigned int)
  __Z15SOUND_SendEventiiiiii   # SOUND_SendEvent(int, int, int, int, int, int)
  __Z16SOUND_PauseSoundi   # SOUND_PauseSound(int)
  __Z16SOUND_StopAllSfxv   # SOUND_StopAllSfx()
  __Z17SOUND_SetChannelsi   # SOUND_SetChannels(int)
  __Z18SOUND_SendEventExtiiiiiii   # SOUND_SendEventExt(int, int, int, int, int, int, int)
  __Z18SOUND_SetSfxStatusj   # SOUND_SetSfxStatus(unsigned int)
  __Z18SOUND_SetSfxVolumej   # SOUND_SetSfxVolume(unsigned int)
  __Z18SOUND_UnpauseSoundi   # SOUND_UnpauseSound(int)
  __Z19SOUND_FadeOutAllSfxv   # SOUND_FadeOutAllSfx()
  __Z19SOUND_SetParamValueijj   # SOUND_SetParamValue(int, unsigned int, unsigned int)
  __Z20SOUND_SetEarPositionfffffffff   # SOUND_SetEarPosition(float, float, float, float, float, float, float, float, float)
  __Z20SOUND_SetMusicStatusj   # SOUND_SetMusicStatus(unsigned int)
  __Z20SOUND_SetMusicVolumej   # SOUND_SetMusicVolume(unsigned int)
  __Z20SOUND_SetReverbLeveli   # SOUND_SetReverbLevel(int)
  __Z21SOUND_FadeSoundEffecti   # SOUND_FadeSoundEffect(int)
  __Z21SOUND_MoveSoundEffectiiii   # SOUND_MoveSoundEffect(int, int, int, int)
  __Z21SOUND_SetGlobalVolumej   # SOUND_SetGlobalVolume(unsigned int)
  __Z21SOUND_SetSoundQualityi   # SOUND_SetSoundQuality(int)
  __Z21SOUND_SetSpeechStatusj   # SOUND_SetSpeechStatus(unsigned int)
  __Z21SOUND_SetSpeechVolumej   # SOUND_SetSpeechVolume(unsigned int)
  __Z21SOUND_StopSoundEffecti   # SOUND_StopSoundEffect(int)
  __Z22SOUND_SetVolumeOnSoundii   # SOUND_SetVolumeOnSound(int, int)
  __Z23SOUND_SetDroppableLevelj   # SOUND_SetDroppableLevel(unsigned int)
  __Z28SOUND_SetOcclusionProcessingi   # SOUND_SetOcclusionProcessing(int)
  __Z28SOUND_SetPauseOn3DSoundsOnlyi   # SOUND_SetPauseOn3DSoundsOnly(int)
  __Z42SOUND_GetCurrentSampleLengthInMillisecondsi   # SOUND_GetCurrentSampleLengthInMilliseconds(int)
  _gAudioSystem
  _gLobbyIslandSoundCount
  _gLobbyIslandSounds
  _gLobbyThunderBank
  _gLocalParkMusicBank
  _gSoundAllocations
  _gSoundBanks
  _gSoundSettings
  _gSoundSystemOn
  _gSoundWanted
)
