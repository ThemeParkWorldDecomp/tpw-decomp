# w70: the sound library's map streamer and containers. The unit shim includes the sources
# with file swaps forced; file fakes supply the corresponding host-endian field bytes.
set(TPW_UNIT_MapStreamer src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_MapStreamer_OWN_CALLLOG ON)
