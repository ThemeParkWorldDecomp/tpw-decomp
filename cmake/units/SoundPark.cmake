# The park sound wrappers (unit w74): the park music, chatter and rain wrappers, SND_DipMusic, SND_SendIndirectedEvent and the sound
# status save and load, with the state in TPSound.cpp. The runtime wrappers they call (SOUND_SendEvent, SOUND_SetParamValue,
# SOUND_FadeSoundEffect), RSSE_GetSoundEventCode, LbFile_Read/Write/Seek, the SamsUtilities swaps and the audio object are fakes in
# the unit file.
# The game builds TbSysCommand objects whose Execute bodies are the library's (unit w79h): tests/fake_syscommands.cpp answers 0 for them.
set(TPW_UNIT_SoundPark src/SoundPark.cpp src/TPSound.cpp tests/fake_syscommands.cpp)
