# Unit v51045: TbStaticFileBank::RegisterBank, TbSoundSampleStore::operator[] and three sound static initialisers.
set(TPW_UNIT_TbStaticFileBank_RegisterBank src/sound/StreamableMpeg2.cpp src/sound/TbMMFileBank.cpp
    src/sound/TbSoundSamplePipe.cpp src/sound/TbSoundCore.cpp src/sound/TbSoundSampleStore.cpp
    src/sound/AudioComponent.cpp src/sound/ChannelState.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCodecLoop.cpp
    src/sound/TbSoundPipe.cpp src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundSampleWAV.cpp platform/arena.cpp)
set(TPW_UNIT_TbStaticFileBank_RegisterBank_KEEP
  __Z24__sinit_TbMMFileBank_cppv   # __sinit_TbMMFileBank_cpp()
  __Z27__sinit_StreamableMpeg2_cppv   # __sinit_StreamableMpeg2_cpp()
  __Z29__sinit_TbSoundSamplePipe_cppv   # __sinit_TbSoundSamplePipe_cpp()
  __ZN12CAtomicValueC1Ev   # CAtomicValue::CAtomicValue()
  __ZN12CAtomicValuemmEv   # CAtomicValue::operator--()
  __ZN12CAtomicValueppEv   # CAtomicValue::operator++()
  __ZN13CChannelState10AddChannelEv   # CChannelState::AddChannel()
  __ZN13CChannelState13RemoveChannelEv   # CChannelState::RemoveChannel()
  __ZN13CChannelState13mChannelStateE   # CChannelState::mChannelState
  __ZN13CChannelState24GetNumberOfChannelsInUseEv   # CChannelState::GetNumberOfChannelsInUse()
  __ZN13CChannelState8InstanceEv   # CChannelState::Instance()
  __ZN13CChannelStateC1Ev   # CChannelState::CChannelState()
  __ZN13CUniqueHandle14_mBitsPerCountE   # CUniqueHandle::_mBitsPerCount
  __ZN13CUniqueHandle8SetIndexEt   # CUniqueHandle::SetIndex(unsigned short)
  __ZN13CUniqueHandleC1EPv   # CUniqueHandle::CUniqueHandle(void*)
  __ZN13CUniqueHandleC1Ev   # CUniqueHandle::CUniqueHandle()
  __ZN13CUniqueHandleppEv   # CUniqueHandle::operator++()
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
  __ZN20CAutoCriticalSectionC1Ev   # CAutoCriticalSection::CAutoCriticalSection()
  __ZN20CAutoCriticalSectionD1Ev   # CAutoCriticalSection::~CAutoCriticalSection()
  __ZN20CAutoCriticalSectioncvPPvEv   # CAutoCriticalSection::operator void**()
  __ZN22TbSoundCriticalSectionC1EPv   # TbSoundCriticalSection::TbSoundCriticalSection(void*)
  __ZN22TbSoundCriticalSectionD1Ev   # TbSoundCriticalSection::~TbSoundCriticalSection()
  __ZN7CLowestC1Ev   # CLowest::CLowest()
  __ZN7CLowestclEjP18TbSoundPipeChannel   # CLowest::operator()(unsigned int, TbSoundPipeChannel*)
  __ZN9TbSpeaker10AutoDelete10_mpSpeakerE   # TbSpeaker::AutoDelete::_mpSpeaker
  __ZN9TbSpeaker10AutoDeleteD1Ev   # TbSpeaker::AutoDelete::~AutoDelete()
  __ZNK12CAtomicValuecviEv   # CAtomicValue::operator int() const
  __ZNK13CUniqueHandle8GetIndexEv   # CUniqueHandle::GetIndex() const
  __ZNK13CUniqueHandlecvPvEv   # CUniqueHandle::operator void*() const
  __ZNK18TbSoundSampleStoreixERK13CUniqueHandle   # TbSoundSampleStore::operator[](CUniqueHandle const&) const
  __ZNK18TbSoundSampleStoreixEj   # TbSoundSampleStore::operator[](unsigned int) const
  _gPoolCleanupBuilt
  _gPoolQueueBuilt
  _gSinitDtor_1003352c
  _gSinitDtor_100336dc
  _gSinitDtor_10033d34
  _gSinitDtor_10033d3c
  _gSinit_1003ebc8
  _gSinit_1003ebe8
  _gSinit_1003ebec
  _gSinit_1003ebf8
  _gSinit_1003f284
  _gSinit_1003f288
  _gSinit_1003f2a8
  _gSinit_1003f2ac
)
