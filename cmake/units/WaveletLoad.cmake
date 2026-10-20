# Unit w86: Bitmap::load_wavelet (src/Bitmap.cpp) and the fast-decoder dispatcher CWaveletImp::Decompress 0x10039694
# (src/Wavelet_decode.cpp, with src/tb_resource_texture.cpp as in the WaveletDecode unit).
set(TPW_UNIT_WaveletLoad src/Bitmap.cpp src/Wavelet_decode.cpp src/tb_resource_texture.cpp)
# The unit file owns the allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_WaveletLoad_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks must not be assumed dead.
cmake_language(DEFER CALL target_compile_options tpw_unit_WaveletLoad PRIVATE -fcheck-new)
