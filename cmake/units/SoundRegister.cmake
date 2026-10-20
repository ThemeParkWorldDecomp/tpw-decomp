# The sound registration layer (unit w65): SOUND_Malloc/Free, SOUND_RegisterCatagory (two forms), SOUND_FreeCatagory and the
# SND_Register*/SND_Free* functions, over the state in TPSound.cpp. The bullfrog strings are real (TbString.cpp, platform/msl.cpp);
# everything else the layer calls is a fake in the unit file.
# The game builds TbSysCommand objects whose Execute bodies are the library's (unit w79h): tests/fake_syscommands.cpp answers 0 for them.
set(TPW_UNIT_SoundRegister src/SoundRegister.cpp src/TPSound.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp tests/fake_syscommands.cpp)
