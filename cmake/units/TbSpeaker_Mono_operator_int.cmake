# Unit v51075: small sound functions (src/sound/Bank.cpp, src/sound/ChannelState.cpp, src/sound/TbBank.cpp, src/sound/TbSoundSampleWAVFile.cpp, src/sound/TimerCodec.cpp, src/sound/UserCallback.cpp) and the existing TbSoundSampleStore::operator[].
# The callees outside the unit are recording fakes in tests/unit_TbSpeaker_Mono_operator_int.cpp.
set(TPW_UNIT_TbSpeaker_Mono_operator_int src/sound/Bank.cpp src/sound/ChannelState.cpp src/sound/TbBank.cpp
    src/sound/TbSoundSampleWAVFile.cpp src/sound/TimerCodec.cpp src/sound/UserCallback.cpp src/sound/TbSoundCore.cpp
    src/sound/TbSoundSampleStore.cpp platform/arena.cpp)
set(TPW_UNIT_TbSpeaker_Mono_operator_int_KEEP
  __Z15TbRectConstructP6TbRect   # TbRectConstruct(TbRect*)
  __Z22__sinit_TimerCodec_cppv   # __sinit_TimerCodec_cpp()
  __Z24__sinit_ChannelState_cppv   # __sinit_ChannelState_cpp()
  __Z24__sinit_UserCallback_cppv   # __sinit_UserCallback_cpp()
  __Z27DestructUserCallbackCleanupP23_CS_TbSoundUserCallbacks   # DestructUserCallbackCleanup(_CS_TbSoundUserCallback*, short)
  __Z32__sinit_TbSoundSampleWAVFile_cppv   # __sinit_TbSoundSampleWAVFile_cpp()
  __Z8NameTextRK23TbDynamicStringTemplateIcE   # NameText(TbDynamicStringTemplate<char> const&)
  __ZN11CRefManagerC1Ev   # CRefManager::CRefManager()
  __ZN16_CS_TbTimerCodec8DestructEPS_s   # _CS_TbTimerCodec::Destruct(_CS_TbTimerCodec*, short)
  __ZN18TbSoundSampleStore14GetFreeChannelER17TbSoundSampleInfo   # TbSoundSampleStore::GetFreeChannel(TbSoundSampleInfo&)
  __ZN18TbSoundSampleStore15CreateNewSampleEPvR17TbSoundSampleInfoRj   # TbSoundSampleStore::CreateNewSample(void*, TbSoundSampleInfo&, unsigned int&)
  __ZN18TbSoundSampleStore15ReleaseAllPipesEv   # TbSoundSampleStore::ReleaseAllPipes()
  __ZN18TbSoundSampleStore15StopAllChannelsEb   # TbSoundSampleStore::StopAllChannels(bool)
  __ZN18TbSoundSampleStore16CChannelIteratorC1EPS_   # TbSoundSampleStore::CChannelIterator::CChannelIterator(TbSoundSampleStore*)
  __ZN18TbSoundSampleStore16CChannelIteratorcvP18TbSoundPipeChannelEv   # TbSoundSampleStore::CChannelIterator::operator TbSoundPipeChannel*()
  __ZN18TbSoundSampleStore16CChannelIteratorppEv   # TbSoundSampleStore::CChannelIterator::operator++()
  __ZN18TbSoundSampleStore16CreateNewControlEPvR17TbSoundSampleInfo   # TbSoundSampleStore::CreateNewControl(void*, TbSoundSampleInfo&)
  __ZN18TbSoundSampleStore18FindChannelAndPipeERK13CUniqueHandleRP17TbSoundSamplePipe   # TbSoundSampleStore::FindChannelAndPipe(CUniqueHandle const&, TbSoundSamplePipe*&)
  __ZN18TbSoundSampleStore18GetPipesFromHandleEPvPP18TbSoundPipeChannelR17TbSoundSampleInfo   # TbSoundSampleStore::GetPipesFromHandle(void*, TbSoundPipeChannel**, TbSoundSampleInfo&)
  __ZN18TbSoundSampleStore20CUsedChannelIteratorC1EPS_   # TbSoundSampleStore::CUsedChannelIterator::CUsedChannelIterator(TbSoundSampleStore*)
  __ZN18TbSoundSampleStore22GetMaxNumberOfChannelsEv   # TbSoundSampleStore::GetMaxNumberOfChannels()
  __ZN18TbSoundSampleStore23GetNumberOfUsedChannelsEv   # TbSoundSampleStore::GetNumberOfUsedChannels()
  __ZN18TbSoundSampleStore24SetNumberOfChannelsToUseERK17TbSoundSystemInfo   # TbSoundSampleStore::SetNumberOfChannelsToUse(TbSoundSystemInfo const&)
  __ZN18TbSoundSampleStore30GetValidChannelIndexFromHandleERK13CUniqueHandleRj   # TbSoundSampleStore::GetValidChannelIndexFromHandle(CUniqueHandle const&, unsigned int&)
  __ZN18TbSoundSampleStore32StopAllChannelsWithVolumeControlEj   # TbSoundSampleStore::StopAllChannelsWithVolumeControl(unsigned int)
  __ZN18TbSoundSampleStore4InitEj   # TbSoundSampleStore::Init(unsigned int)
  __ZN18TbSoundSampleStoreC1EP11TbSoundCorej   # TbSoundSampleStore::TbSoundSampleStore(TbSoundCore*, unsigned int)
  __ZN18TbSoundSampleStoreD1Ev   # TbSoundSampleStore::~TbSoundSampleStore()
  __ZN21_CS__TbSoundSampleWAV8DestructEPS_s   # _CS__TbSoundSampleWAV::Destruct(_CS__TbSoundSampleWAV*, short)
  __ZN25_CS__TbSoundSampleWAVFile8DestructEPS_s   # _CS__TbSoundSampleWAVFile::Destruct(_CS__TbSoundSampleWAVFile*, short)
  __ZN5tbstd4fabsEd   # tbstd::fabs(double)
  __ZN5tbstd4fabsEf   # tbstd::fabs(float)
  __ZN5tbstd4powfEff   # tbstd::powf(float, float)
  __ZN5tbstd5fabsfEf   # tbstd::fabsf(float)
  __ZN9TbSpeaker22ConfigSetModuleSpeakerEPNS_6ModuleE   # TbSpeaker::ConfigSetModuleSpeaker(TbSpeaker::Module*)
  __ZNK16TbStaticFileBank6IsTypeEj   # TbStaticFileBank::IsType(unsigned int) const
  __ZNK18TbSoundSampleStoreixERK13CUniqueHandle   # TbSoundSampleStore::operator[](CUniqueHandle const&) const
  __ZNK18TbSoundSampleStoreixEj   # TbSoundSampleStore::operator[](unsigned int) const
  __ZNK4Bank5CBank13GetSampleNameEj   # Bank::CBank::GetSampleName(unsigned int) const
  __ZNK4Bank5CBank15GetSampleLengthEj   # Bank::CBank::GetSampleLength(unsigned int) const
  __ZNK4Bank5CBank18GetNumberOfSamplesEv   # Bank::CBank::GetNumberOfSamples() const
  __ZNK4Bank5CBank6IsTypeEj   # Bank::CBank::IsType(unsigned int) const
  __ZNK9TbSpeaker10HeadphonescviEv   # TbSpeaker::Headphones::operator int() const
  __ZNK9TbSpeaker4MonocviEv   # TbSpeaker::Mono::operator int() const
  __ZNK9TbSpeaker6StereocviEv   # TbSpeaker::Stereo::operator int() const
  _gCS_TbSoundSampleWAVFileNode
  _gCS_TbSoundSampleWAVFileObject
  _gCS_TbSoundUserCallbackNode
  _gCS_TbSoundUserCallbackObject
  _gCS_TbTimerCodecNode
  _gCS_TbTimerCodecObject
)
