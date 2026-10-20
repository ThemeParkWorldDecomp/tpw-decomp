# Unit w79e: the sound manager the game holds (TbSoundManager), over the real core (w79b: TbSoundCore, CAudioComponent and the command
# constructors), the info records (w77), TbRefRelease (w78), the handle values (w79a) and the bullfrog strings. The unit file
# includes tests/calllog.cpp itself (it hooks the allocation observer). Fakes there: the device module, the sample store and the
# channel getters (as in the w79b unit), the command Execute bodies (w79d), TbMapStreamer::Instance and SetDroppable, the storage
# and file imports, CFileOperation::GetBankPath, TbSoundBankInfo's file constructor and CBankFactory.
set(TPW_UNIT_SoundManager src/TbSoundManager.cpp src/TbSoundCore.cpp src/CAudioComponent.cpp src/snd_commands.cpp src/TbSoundInfo.cpp
    src/TbRefRelease.cpp src/TbSoundSystemModule.cpp src/snd_handles.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp
    platform/arena.cpp)
set(TPW_UNIT_SoundManager_OWN_CALLLOG ON)
