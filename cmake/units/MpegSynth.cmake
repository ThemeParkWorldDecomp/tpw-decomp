# The sound library's MPEG audio decoder, part 2 (unit w71b): PolySynth, dct64, WRITE_SAMPLE, Decode_Layer2, II_step_two and
# II_step_twoMMX. Linked with part 1 (unit w71a), whose pool, bit reader and header code the real functions call; no fakes.
set(TPW_UNIT_MpegSynth src/TbMpegSynth.cpp src/TbMpegBase.cpp)
