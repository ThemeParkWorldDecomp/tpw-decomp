# Unit w86: Bitmap::load_wavelet (src/engine/Bitmap.cpp) and the fast-decoder dispatcher CWaveletImp::Decompress 0x10039694
# (src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp, with src/engine/tb_resource_texture.cpp as in the WaveletDecode unit).
set(TPW_UNIT_WaveletLoad src/engine/Bitmap.cpp src/engine/EngineUtil.cpp src/engine/Wavelet.cpp
    src/engine/tb_resource_texture.cpp)
set(TPW_UNIT_WaveletLoad_KEEP
  __Z10read_resetv   # read_reset()
  __Z14LzssDecompressPvS_   # LzssDecompress(void*, void*)
  __Z24ConvertSurfaceSizeToFlagi   # ConvertSurfaceSizeToFlag(int)
  __Z9read_codej   # read_code(unsigned int)
  __ZN11CWaveletImp10DecompressEiiiiPvS0_S0_ffff   # CWaveletImp::Decompress(int, int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp10DecompressEiiiiiiiPvS0_S0_ffff   # CWaveletImp::Decompress(int, int, int, int, int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp10DecompressEjjbPKhPhfff   # CWaveletImp::Decompress(unsigned int, unsigned int, bool, unsigned char const*, unsigned char*, float, float, float)
  __ZN11CWaveletImp14Decompress_v13EiiiPvS0_S0_ffff   # CWaveletImp::Decompress_v13(int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp20ReverseTransformVertEPKfPfii   # CWaveletImp::ReverseTransformVert(float const*, float*, int, int)
  __ZN16CTextureSettings12GetAlphaMaskEv   # CTextureSettings::GetAlphaMask()
  __ZN16CTextureSettings15OutputDebugInfoEv   # CTextureSettings::OutputDebugInfo()
  __ZN16CTextureSettings16CalculateUVFixupEv   # CTextureSettings::CalculateUVFixup()
  __ZN16CTextureSettingsC1Ev   # CTextureSettings::CTextureSettings()
  __ZN6Bitmap11colour_fillEhhhh   # Bitmap::colour_fill(unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap11shadowblendERS_   # Bitmap::shadowblend(Bitmap&)
  __ZN6Bitmap12load_waveletEPvP8CWavelet   # Bitmap::load_wavelet(void*, CWavelet*)
  __ZN6Bitmap17swizzle_for_gimexEv   # Bitmap::swizzle_for_gimex()
  __ZN6Bitmap4loadEPv   # Bitmap::load(void*)
  __ZN6Bitmap5clearEv   # Bitmap::clear()
  __ZN6Bitmap7copy_toERS_   # Bitmap::copy_to(Bitmap&)
  __ZN6Bitmap7reallocEjjj   # Bitmap::realloc(unsigned int, unsigned int, unsigned int)
  __ZN6Bitmap8alphabltERS_jj   # Bitmap::alphablt(Bitmap&, unsigned int, unsigned int)
  __ZN6Bitmap9colourbltERS_hhhh   # Bitmap::colourblt(Bitmap&, unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap9descimateERS_   # Bitmap::descimate(Bitmap&)
  _gLzssBitBuffer
  _gLzssBitCount
  _gLzssDest
  _gLzssSource
  _gLzssSourcePos
  _gLzssUnused
  _gWaveletH0
  _gWaveletH1
  _gWaveletH2
  _gWaveletH3
  _gWaveletPlanes
  _gei_TextureSettings
  __ZTS8CIgArrayIffE   # template class CIgArray<float, float> (explicit instantiation)
  tmpl_1103eef775d0@engine/Wavelet.cpp   # ~CIgArray<TYPE, ARG_TYPE> (a template definition src/Wavelet_decode.cpp had before the regroup)
  tmpl_3e204a388746@engine/Wavelet.cpp   # SetSize (a template definition src/Wavelet_decode.cpp had before the regroup)
  tmpl_85479d03856e@engine/Wavelet.cpp   # CIgArray<TYPE, ARG_TYPE> (a template definition src/Wavelet_decode.cpp had before the regroup)
  tmpl_d3d5f0f93447@engine/Wavelet.cpp   # GetData (a template definition src/Wavelet_decode.cpp had before the regroup)
)
# The unit file owns the allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_WaveletLoad_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks must not be assumed dead.
cmake_language(DEFER CALL target_compile_options tpw_unit_WaveletLoad PRIVATE -fcheck-new)
