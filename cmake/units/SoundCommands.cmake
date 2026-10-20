# Unit w79d: the SystemCommand Execute bodies of the sound library (src/snd_commands_execute.cpp). Real: the command constructors
# (w79b), the info records and positions (w77), the device-module base and the handle values (w79a), the pipe base (w78) and
# TbRefRelease. The unit file fakes the access object's core call, the device module, the sample store and its iterator, the
# channels and SamsUtilities::UDebug.
set(TPW_UNIT_SoundCommands src/snd_commands_execute.cpp src/snd_commands.cpp src/TbSoundInfo.cpp src/TbSoundSystemModule.cpp
    src/snd_handles.cpp src/CAudioComponent.cpp src/TbRefRelease.cpp src/TbSoundPipe.cpp)
