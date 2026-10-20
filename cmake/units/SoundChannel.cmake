# Unit w79c: the sound library's pipe channel and sample store. The unit file includes src/TbSoundPipeChannel.cpp,
# src/TbSoundSampleStore.cpp and src/snd_handles.cpp itself (the memset, the double conversion, TbSoundCore::StopAndReleasePipe
# and CCodecFactory are recording fakes there); the pipe base, the sample info records and TbRefRelease (the base of the pipe
# base's owner) are linked real. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_SoundChannel src/TbSoundPipe.cpp src/TbSoundInfo.cpp src/TbRefRelease.cpp)
set(TPW_UNIT_SoundChannel_OWN_CALLLOG ON)
