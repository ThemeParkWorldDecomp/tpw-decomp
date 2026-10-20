# Unit w79i: the sound library's sample callback pipes (TbSoundUserCallback, _TbSoundSampleCallback,
# _TbSoundSampleCompletedCallback, CAutoDetach, their pools and cleanup objects). The unit file includes tests/calllog.cpp itself
# (it hooks the allocation observer); real beside them are the pipe base (w78, src/TbSoundPipe.cpp), the sample info setters
# (w77, src/TbSoundInfo.cpp), the timer codec (w80, src/TbSoundCodecs.cpp) and TbRefRelease. Fakes in the unit file: the next
# codec behind a pipe, the TbSndCallback, the open file class and the clock.
set(TPW_UNIT_SoundCallbackPipes src/TbSoundSampleCallback.cpp src/TbSoundPipe.cpp src/TbSoundInfo.cpp src/TbSoundCodecs.cpp
    src/TbRefRelease.cpp)
set(TPW_UNIT_SoundCallbackPipes_OWN_CALLLOG ON)
