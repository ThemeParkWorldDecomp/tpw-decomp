# The sound library's MPEG file player (unit w80c): CMpegMacFilePlayerUnpooled, its pool UPooled<..., 10> and the pool's queue.
# CMpegBase (src/sound/FileAccess.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp, unit w71a) is the real base class; PolySynth and Decode_Layer2 (unit w71b) are fakes in the unit
# file, which includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_MpegFilePlayer src/sound/FileAccess.cpp src/sound/LbMpeg.cpp src/sound/MpegDecode.cpp
    src/sound/CSprocketsModule.cpp src/sound/StreamableMpeg2.cpp)
set(TPW_UNIT_MpegFilePlayer_KEEP
  __Z14MpegMultiplierii   # MpegMultiplier(int, int)
  __Z21MpegIntegerAllocationi   # MpegIntegerAllocation(int)
  __Z6muldiviii   # muldiv(int, int, int)
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE10gPoolQueueE   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::gPoolQueue
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE11gPoolTargetE   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::gPoolTarget
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE12gPoolCleanupE   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::gPoolCleanup
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE13StaticCleanupD1Ev   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::StaticCleanup::~StaticCleanup()
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE13gPoolKeepFullE   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::gPoolKeepFull
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE15SetPoolKeepFullEh   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::SetPoolKeepFull(unsigned char)
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE17MaintainPoolLevelEv   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::MaintainPoolLevel()
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE18MakeNewPoolElementEv   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::MakeNewPoolElement()
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE20ReportPoolStatisticsEv   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::ReportPoolStatistics()
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE21IsPoolObjectAvailableEPKS1_   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::IsPoolObjectAvailable(CMpegMacFilePlayerUnpooled const*)
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EE9ClearPoolEh   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::ClearPool(unsigned char)
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EEC1Ev   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::UPooled()
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EEdlEPvm   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::operator delete(void*, unsigned long)
  __ZN13SamsUtilities7UPooledI26CMpegMacFilePlayerUnpooledLi10EEnwEm   # SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::operator new(unsigned long)
  __ZN13_CS_CMpegBaseD1Ev   # _CS_CMpegBase::~_CS_CMpegBase()
  __ZN22CMpegBase_DecodeMethodC1Ev   # CMpegBase_DecodeMethod::CMpegBase_DecodeMethod()
  __ZN22CMpegBase_DecodeMethodD1Ev   # CMpegBase_DecodeMethod::~CMpegBase_DecodeMethod()
  __ZN26CMpegMacFilePlayerUnpooled10NextBufferEv   # CMpegMacFilePlayerUnpooled::NextBuffer()
  __ZN26CMpegMacFilePlayerUnpooled12DecodeHeaderEv   # CMpegMacFilePlayerUnpooled::DecodeHeader()
  __ZN26CMpegMacFilePlayerUnpooled12ReadBufferedEPvj   # CMpegMacFilePlayerUnpooled::ReadBuffered(void*, unsigned int)
  __ZN26CMpegMacFilePlayerUnpooled13RequestBufferEib   # CMpegMacFilePlayerUnpooled::RequestBuffer(int, bool)
  __ZN26CMpegMacFilePlayerUnpooled4OpenEPN3bfl10FileSystem5CFileEiiPc   # CMpegMacFilePlayerUnpooled::Open(bfl::FileSystem::CFile*, int, int, char*)
  __ZN26CMpegMacFilePlayerUnpooled5CloseEv   # CMpegMacFilePlayerUnpooled::Close()
  __ZN26CMpegMacFilePlayerUnpooled8SeekBitsEi   # CMpegMacFilePlayerUnpooled::SeekBits(int)
  __ZN26CMpegMacFilePlayerUnpooled9GetHeaderEv   # CMpegMacFilePlayerUnpooled::GetHeader()
  __ZN9CMpegBase10ClearCacheEv   # CMpegBase::ClearCache()
  __ZN9CMpegBase10CloseLayerEv   # CMpegBase::CloseLayer()
  __ZN9CMpegBase10CloseSynthEv   # CMpegBase::CloseSynth()
  __ZN9CMpegBase11Init_TablesEv   # CMpegBase::Init_Tables()
  __ZN9CMpegBase12Close_TablesEv   # CMpegBase::Close_Tables()
  __ZN9CMpegBase12DecodeHeaderEv   # CMpegBase::DecodeHeader()
  __ZN9CMpegBase13Decode_Layer1EPs   # CMpegBase::Decode_Layer1(short*)
  __ZN9CMpegBase13ProcessHeaderEj   # CMpegBase::ProcessHeader(unsigned int)
  __ZN9CMpegBase13get_sb_stereoEiPA2_cPA32_jPsPi   # CMpegBase::get_sb_stereo(int, char (*) [2], unsigned int (*) [32], short*, int*)
  __ZN9CMpegBase13mLowCMpegBaseE   # CMpegBase::mLowCMpegBase
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
  __ZN9CMpegBaseC1Ev   # CMpegBase::CMpegBase()
  __ZN9CMpegBaseD1Ev   # CMpegBase::~CMpegBase()
  __ZN9CMpegBasedlEPvm   # CMpegBase::operator delete(void*, unsigned long)
  __ZN9CMpegBasenwEm   # CMpegBase::operator new(unsigned long)
  _gMpegLayer1Alloc
  _gMpegLayer1Sample
  _gMpegLayer1Scale
  _using_MMX
  __ZN12_GLOBAL__N_110gQueueLockE   # gQueueLock (static object)
  __ZN12_GLOBAL__N_113sDecodeMethodE   # sDecodeMethod (static object)
  __ZN12_GLOBAL__N_18sCleanupE   # sCleanup (static object)
  __ZTSN13SamsUtilities19UInterruptSafeQueueIA28064_hEE   # template class UInterruptSafeQueue<unsigned char[sizeof(CMpegMacFilePlayerUnpooled)]> (explicit instantiation)
  tmpl_078422721b4e@sound/LbMpeg.cpp   # GetNext (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_55adcb76f509@sound/LbMpeg.cpp   # GetData (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_7ae756b0efa6@sound/LbMpeg.cpp   # GetSize (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_91ca7e73dbfc@sound/LbMpeg.cpp   # Remove (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_987057d430ed@sound/LbMpeg.cpp   # GetHead (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_ad06a2bba23d@sound/LbMpeg.cpp   # Add (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_bbe148d1c7f0@sound/LbMpeg.cpp   # GetElementFromDataPointer (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_da5de86c59a2@sound/LbMpeg.cpp   # UInterruptSafeQueue<Data> (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
  tmpl_f25d7d25e348@sound/LbMpeg.cpp   # Element (a template definition src/TbMpegFilePlayer.cpp had before the regroup)
)
set(TPW_UNIT_MpegFilePlayer_OWN_CALLLOG ON)
