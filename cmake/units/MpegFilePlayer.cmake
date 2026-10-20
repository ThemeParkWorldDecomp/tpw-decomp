# The sound library's MPEG file player (unit w80c): CMpegMacFilePlayerUnpooled, its pool UPooled<..., 10> and the pool's queue.
# CMpegBase (src/TbMpegBase.cpp, unit w71a) is the real base class; PolySynth and Decode_Layer2 (unit w71b) are fakes in the unit
# file, which includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_MpegFilePlayer src/TbMpegBase.cpp src/TbMpegFilePlayer.cpp)
set(TPW_UNIT_MpegFilePlayer_OWN_CALLLOG ON)
