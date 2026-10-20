# Fast transforms share the decoder's real planes and CIgArray, and use the real zlib bridge.
set(TPW_UNIT_WaveletFast src/Wavelet_fast.cpp src/Wavelet_decode.cpp src/lb_zlib.cpp src/tb_resource_texture.cpp)
set(TPW_UNIT_WaveletFast_OWN_CALLLOG ON)
cmake_language(DEFER CALL target_link_libraries tpw_unit_WaveletFast PRIVATE ZLIB::ZLIB)
