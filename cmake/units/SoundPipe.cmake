# Unit w78: the sound library's sample pipe base (TbSoundSamplePipe, TbSoundSPCodec, TbSoundSampleCodec). The unit file includes
# tests/calllog.cpp itself (it hooks the allocation observer); TbRefRelease (real) is the base of the fake owner. The clock
# (LbTime_GetClock) and the codecs' callees are fakes in the unit file.
set(TPW_UNIT_SoundPipe src/TbSoundPipe.cpp src/TbRefRelease.cpp)
set(TPW_UNIT_SoundPipe_OWN_CALLLOG ON)
