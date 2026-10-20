# The sound runtime (unit w66): SOUND_Init, SOUND_Close, SOUND_Tick, SOUND_SendEvent and the other wrappers over the abstract audio
# object, with the state in TPSound.cpp. Everything the runtime calls outside the unit (CreateAudioObject, the sams keyboard,
# GAME_ASSERTMSG) is a fake in the unit file; gSoundBalance is raw storage there.
# The game builds TbSysCommand objects whose Execute bodies are the library's (unit w79h): tests/fake_syscommands.cpp answers 0 for them.
set(TPW_UNIT_SoundRuntime src/SoundRuntime.cpp src/TPSound.cpp tests/fake_syscommands.cpp)
