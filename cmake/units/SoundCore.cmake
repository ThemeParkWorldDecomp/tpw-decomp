# Unit w79b: the sound library's core (TbSoundCore, CAudioComponent, CModuleFactory, CMemoryCopyInit and the SystemCommand
# constructors). Real: the info records (w77), TbRefRelease (w78), the device-module base and the handle values (w79a). Fakes in
# the unit file: the sample store (w79c, placeholder header), the channel getters (w79c), CSprocketsModule (p-sound) and the
# command Execute bodies (w79d). The unit file includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_SoundCore src/TbSoundCore.cpp src/CAudioComponent.cpp src/snd_commands.cpp src/TbSoundInfo.cpp src/TbRefRelease.cpp
    src/TbSoundSystemModule.cpp src/snd_handles.cpp)
set(TPW_UNIT_SoundCore_OWN_CALLLOG ON)
