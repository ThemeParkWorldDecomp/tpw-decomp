# The sound library's MPEG audio decoder, part 2 (unit w71b): PolySynth, dct64, WRITE_SAMPLE, Decode_Layer2, II_step_two and
# II_step_twoMMX. Linked with part 1 (unit w71a), whose pool, bit reader and header code the real functions call; no fakes.
set(TPW_UNIT_MpegSynth src/sound/MpegDecode.cpp src/sound/FileAccess.cpp src/sound/LbMpeg.cpp)
set(TPW_UNIT_MpegSynth_KEEP
  __Z10MpegDecwinv   # MpegDecwin()
  __Z12WRITE_SAMPLEPsf   # WRITE_SAMPLE(short*, float)
  __Z14MpegAllocTablei   # MpegAllocTable(int)
  __Z14MpegGroupTablei   # MpegGroupTable(int)
  __Z14MpegMultiplierii   # MpegMultiplier(int, int)
  __Z15MpegDct64Cosinei   # MpegDct64Cosine(int)
  __Z17MpegLayer2SbLimiti   # MpegLayer2SbLimit(int)
  __Z19MpegLayer2Translatev   # MpegLayer2Translate()
  __Z21MpegAllocTableEntriesi   # MpegAllocTableEntries(int)
  __Z21MpegIntegerAllocationi   # MpegIntegerAllocation(int)
  __Z5dct64PfS_S_   # dct64(float*, float*, float*)
  __Z6muldiviii   # muldiv(int, int, int)
  __ZN13_CS_CMpegBaseD1Ev   # _CS_CMpegBase::~_CS_CMpegBase()
  __ZN22CMpegBase_DecodeMethodC1Ev   # CMpegBase_DecodeMethod::CMpegBase_DecodeMethod()
  __ZN22CMpegBase_DecodeMethodD1Ev   # CMpegBase_DecodeMethod::~CMpegBase_DecodeMethod()
  __ZN9CMpegBase10ClearCacheEv   # CMpegBase::ClearCache()
  __ZN9CMpegBase10CloseLayerEv   # CMpegBase::CloseLayer()
  __ZN9CMpegBase10CloseSynthEv   # CMpegBase::CloseSynth()
  __ZN9CMpegBase11II_step_twoEPcPA2_A32_fS0_iPK8al_table   # CMpegBase::II_step_two(char*, float (*) [2][32], char*, int, al_table const*)
  __ZN9CMpegBase11Init_TablesEv   # CMpegBase::Init_Tables()
  __ZN9CMpegBase12Close_TablesEv   # CMpegBase::Close_Tables()
  __ZN9CMpegBase12DecodeHeaderEv   # CMpegBase::DecodeHeader()
  __ZN9CMpegBase13Decode_Layer1EPs   # CMpegBase::Decode_Layer1(short*)
  __ZN9CMpegBase13Decode_Layer2EPs   # CMpegBase::Decode_Layer2(short*)
  __ZN9CMpegBase13ProcessHeaderEj   # CMpegBase::ProcessHeader(unsigned int)
  __ZN9CMpegBase13get_sb_stereoEiPA2_cPA32_jPsPi   # CMpegBase::get_sb_stereo(int, char (*) [2], unsigned int (*) [32], short*, int*)
  __ZN9CMpegBase13mLowCMpegBaseE   # CMpegBase::mLowCMpegBase
  __ZN9CMpegBase14II_step_twoMMXEPiPcPA2_A32_sS1_PK8al_table   # CMpegBase::II_step_twoMMX(int*, char*, short (*) [2][32], char*, al_table const*)
  __ZN9CMpegBase14mHighCMpegBaseE   # CMpegBase::mHighCMpegBase
  __ZN9CMpegBase15mFirstCMpegBaseE   # CMpegBase::mFirstCMpegBase
  __ZN9CMpegBase15mLevelCMpegBaseE   # CMpegBase::mLevelCMpegBase
  __ZN9CMpegBase15mTotalCMpegBaseE   # CMpegBase::mTotalCMpegBase
  __ZN9CMpegBase17mCurrentCMpegBaseE   # CMpegBase::mCurrentCMpegBase
  __ZN9CMpegBase4OpenEPvi   # CMpegBase::Open(void*, int)
  __ZN9CMpegBase4OpenEji   # CMpegBase::Open(unsigned int, int)
  __ZN9CMpegBase4ReadEPvi   # CMpegBase::Read(void*, int)
  __ZN9CMpegBase5CloseEv   # CMpegBase::Close()
  __ZN9CMpegBase5ResetEv   # CMpegBase::Reset()
  __ZN9CMpegBase5TopUpEv   # CMpegBase::TopUp()
  __ZN9CMpegBase6DecodeEPs   # CMpegBase::Decode(short*)
  __ZN9CMpegBase7GetBitsEi   # CMpegBase::GetBits(int)
  __ZN9CMpegBase8SeekBitsEi   # CMpegBase::SeekBits(int)
  __ZN9CMpegBase8SetLevelEi   # CMpegBase::SetLevel(int)
  __ZN9CMpegBase9GetHeaderEj   # CMpegBase::GetHeader(unsigned int)
  __ZN9CMpegBase9GetHeaderEv   # CMpegBase::GetHeader()
  __ZN9CMpegBase9OpenLayerEv   # CMpegBase::OpenLayer()
  __ZN9CMpegBase9OpenSynthEv   # CMpegBase::OpenSynth()
  __ZN9CMpegBase9PolySynthEPsPf   # CMpegBase::PolySynth(short*, float*)
  __ZN9CMpegBaseC1Ev   # CMpegBase::CMpegBase()
  __ZN9CMpegBaseD1Ev   # CMpegBase::~CMpegBase()
  __ZN9CMpegBasedlEPvm   # CMpegBase::operator delete(void*, unsigned long)
  __ZN9CMpegBasenwEm   # CMpegBase::operator new(unsigned long)
  _gMpegDctBuf
  _gMpegL2Alloc
  _gMpegL2Fraction
  _gMpegL2Scale
  _gMpegL2Select
  _gMpegLayer1Alloc
  _gMpegLayer1Sample
  _gMpegLayer1Scale
  _using_MMX
  __ZN12_GLOBAL__N_113sDecodeMethodE   # sDecodeMethod (static object)
  __ZN12_GLOBAL__N_18sCleanupE   # sCleanup (static object)
)
