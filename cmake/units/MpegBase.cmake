# The sound library's MPEG audio decoder object, part 1 (unit w71a): CMpegBase's pool, bit reader, header parser, frame reader
# and layer I decoder. PolySynth and Decode_Layer2 (unit w71b) are fakes in the unit file, which includes tests/calllog.cpp
# itself (it hooks the allocation observer).
set(TPW_UNIT_MpegBase src/TbMpegBase.cpp)
set(TPW_UNIT_MpegBase_OWN_CALLLOG ON)
