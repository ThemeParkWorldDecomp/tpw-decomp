set(TPW_UNIT_WaveletDecodeV12 src/engine/Wavelet.cpp src/engine/EngineUtil.cpp src/engine/tb_resource_texture.cpp)
set(TPW_UNIT_WaveletDecodeV12_KEEP
  __Z10read_resetv   # read_reset()
  __Z14LzssDecompressPvS_   # LzssDecompress(void*, void*)
  __Z24ConvertSurfaceSizeToFlagi   # ConvertSurfaceSizeToFlag(int)
  __Z9read_codej   # read_code(unsigned int)
  __ZN11CWaveletImp10DecompressEiiiiPvS0_S0_ffff   # CWaveletImp::Decompress(int, int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp10DecompressEiiiiiiiPvS0_S0_ffff   # CWaveletImp::Decompress(int, int, int, int, int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp10DecompressEjjbPKhPhfff   # CWaveletImp::Decompress(unsigned int, unsigned int, bool, unsigned char const*, unsigned char*, float, float, float)
  __ZN11CWaveletImp14Decompress_v12EiiiPvS0_S0_ffff   # CWaveletImp::Decompress_v12(int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp14Decompress_v13EiiiPvS0_S0_ffff   # CWaveletImp::Decompress_v13(int, int, int, void*, void*, void*, float, float, float, float)
  __ZN11CWaveletImp20ReverseTransformVertEPKfPfii   # CWaveletImp::ReverseTransformVert(float const*, float*, int, int)
  __ZN16CTextureSettings12GetAlphaMaskEv   # CTextureSettings::GetAlphaMask()
  __ZN16CTextureSettings15OutputDebugInfoEv   # CTextureSettings::OutputDebugInfo()
  __ZN16CTextureSettings16CalculateUVFixupEv   # CTextureSettings::CalculateUVFixup()
  __ZN16CTextureSettingsC1Ev   # CTextureSettings::CTextureSettings()
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
