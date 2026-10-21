# Unit v51112: small sound-program functions (src/sound/AudioPlaceHolder.cpp, src/sound/PlaceHolderSentence.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleMPEG2.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSampleWAV.cpp). The callees outside the
# unit are recording fakes in tests/unit_LbMaths_RangedRandomSlow.cpp. LbMaths_Random is tested where it is.
set(TPW_UNIT_LbMaths_RangedRandomSlow src/sound/AudioPlaceHolder.cpp src/sound/PlaceHolderSentence.cpp
    src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleMPEG2.cpp
    src/sound/TbSoundSamplePipe.cpp src/sound/TbSoundSampleWAV.cpp)
set(TPW_UNIT_LbMaths_RangedRandomSlow_KEEP
  _MslSeedGlue1002eaac
  _SoundHeapDelete
  _SoundHeapDeleteArray
  _SoundHeapNewPointers
  __Z10LbMaxULongRKjS0_   # LbMaxULong(unsigned int const&, unsigned int const&)
  __Z10LbMinFloatRKfS0_   # LbMinFloat(float const&, float const&)
  __Z12TbRectAssignP6TbRectPKS_   # TbRectAssign(TbRect*, TbRect const*)
  __Z14CFileWriteMainPN3bfl10FileSystem5CFileEPKvj   # CFileWriteMain(bfl::FileSystem::CFile*, void const*, unsigned int)
  __Z14LbMaths_RandomRi   # LbMaths_Random(int&)
  __Z20DestructStandInMPEG2Pvs   # DestructStandInMPEG2(void*, short)
  __Z21CFormatToWaveFormatExP7CFormat   # CFormatToWaveFormatEx(CFormat*)
  __Z23DestructStandInCallbackPvs   # DestructStandInCallback(void*, short)
  __Z24DestructStandInCodecLoopPvs   # DestructStandInCodecLoop(void*, short)
  __Z24LbMaths_RangedRandomSlowRij   # LbMaths_RangedRandomSlow(int&, unsigned int)
  __Z25OperatorNewPlacementSoundjPv   # OperatorNewPlacementSound(unsigned int, void*)
  __Z27CAutoArrayPtrPlaceHolderGetPK13CAutoArrayPtrIP17CAudioPlaceHolderE   # CAutoArrayPtrPlaceHolderGet(CAutoArrayPtr<CAudioPlaceHolder*> const*)
  __Z28CAutoArrayPtrPlaceHolderCtorP13CAutoArrayPtrIP17CAudioPlaceHolderEj   # CAutoArrayPtrPlaceHolderCtor(CAutoArrayPtr<CAudioPlaceHolder*>*, unsigned int)
  __Z28CAutoArrayPtrPlaceHolderDtorP13CAutoArrayPtrIP17CAudioPlaceHolderEs   # CAutoArrayPtrPlaceHolderDtor(CAutoArrayPtr<CAudioPlaceHolder*>*, short)
  __Z28__sinit_AudioPlaceHolder_cppv   # __sinit_AudioPlaceHolder_cpp()
  __Z28__sinit_TbSoundCodecLoop_cppv   # __sinit_TbSoundCodecLoop_cpp()
  __Z28__sinit_TbSoundSampleWAV_cppv   # __sinit_TbSoundSampleWAV_cpp()
  __Z30__sinit_TbSoundSampleMPEG2_cppv   # __sinit_TbSoundSampleMPEG2_cpp()
  __Z33__sinit_TbSoundSampleCallback_cppv   # __sinit_TbSoundSampleCallback_cpp()
  __Z8LbMinIntRKiS0_   # LbMinInt(int const&, int const&)
  __Z9LbMaxLongRKiS0_   # LbMaxLong(int const&, int const&)
  __Z9LbMinUIntRKjS0_   # LbMinUInt(unsigned int const&, unsigned int const&)
  __ZN13SamsUtilities7UBinary11RotateRightIiEET_S2_S2_   # int SamsUtilities::UBinary::RotateRight<int>(int, int)
  __ZN23TbSoundSampleStoreSound17IteratorIncrementEPNS_15ChannelIteratorEj   # TbSoundSampleStoreSound::IteratorIncrement(TbSoundSampleStoreSound::ChannelIterator*, unsigned int)
  __ZN3bfl10FileSystem16CDiscFileStorage12DestroySoundEPS1_s   # bfl::FileSystem::CDiscFileStorage::DestroySound(bfl::FileSystem::CDiscFileStorage*, short)
  __ZN3bfl5maths16RangedRandomSlowERij   # bfl::maths::RangedRandomSlow(int&, unsigned int)
  __ZN3bfl5maths6RandomERi   # bfl::maths::Random(int&)
  __ZN5tbstd4logfEf   # tbstd::logf(float)
  _gCS_TbSoundCodecLoopNode
  _gCS_TbSoundCodecLoopObject
  _gCS_TbSoundSampleCallbackNode
  _gCS_TbSoundSampleCallbackObject
  _gCS_TbSoundSampleMPEG2Node
  _gCS_TbSoundSampleMPEG2Object
  _gCS_TbSoundSampleWAVNode
  _gCS_TbSoundSampleWAVObject
)
