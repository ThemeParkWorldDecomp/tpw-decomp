# Unit w80: the sound library's four small codecs (_TbSoundSampleWAV, _TbSoundSampleWAVFile, _TbSoundCodecLoop, TbTimerCodec) and
# CMemoryCopy::Copy. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer); the codecs derive from
# unit w78's classes (src/TbSoundPipe.cpp) and TbRefRelease (real) is the base of the fake owner. The clock (LbTime_GetClock),
# the file (bfl::FileSystem::CFile) and the codec behind a loop are fakes in the unit file.
set(TPW_UNIT_SoundCodecs src/TbSoundCodecs.cpp src/TbSoundPipe.cpp src/TbRefRelease.cpp)
set(TPW_UNIT_SoundCodecs_OWN_CALLLOG ON)
