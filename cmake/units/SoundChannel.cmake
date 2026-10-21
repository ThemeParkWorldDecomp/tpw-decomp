# Unit w79c: the sound library's pipe channel and sample store. The unit file includes src/sound/TbSoundPipeChannel.cpp,
# src/sound/TbSoundSampleStore.cpp and src/sound/ChannelState.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundPipe.cpp, src/sound/AudioComponent.cpp, src/sound/SystemCommand.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundSampleWAV.cpp itself (the memset, the double conversion, TbSoundCore::StopAndReleasePipe
# and CCodecFactory are recording fakes there); the pipe base, the sample info records and TbRefRelease (the base of the pipe
# base's owner) are linked real. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_SoundChannel src/sound/StreamableMpeg2.cpp src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleCodec.cpp src/sound/TbSoundSamplePipe.cpp
    src/sound/TbSoundSystemInfo.cpp src/sound/AudioPlaceHolder.cpp src/sound/SystemCommand.cpp
    src/sound/TbSoundCore.cpp src/sound/TbSoundManager.cpp src/sound/TbSoundPipeChannel.cpp
    src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp src/sound/TbSoundSampleStore.cpp
    src/sound/TbSysCommand.cpp src/bullfrog/TbRefRelease.cpp)
set(TPW_UNIT_SoundChannel_KEEP
  __ZN12TbRefRelease6AddRefEv   # TbRefRelease::AddRef()
  __ZN12TbRefRelease7ReleaseEv   # TbRefRelease::Release()
  __ZN12TbRefReleaseC1Ev   # TbRefRelease::TbRefRelease()
  __ZN12TbRefReleaseD1Ev   # TbRefRelease::~TbRefRelease()
  __ZN14TbSoundSPCodec10GetBitRateEv   # TbSoundSPCodec::GetBitRate()
  __ZN14TbSoundSPCodec11GetChannelsEv   # TbSoundSPCodec::GetChannels()
  __ZN14TbSoundSPCodec11GetDataSizeEv   # TbSoundSPCodec::GetDataSize()
  __ZN14TbSoundSPCodec12GetDataStartEv   # TbSoundSPCodec::GetDataStart()
  __ZN14TbSoundSPCodec13GetSampleRateEv   # TbSoundSPCodec::GetSampleRate()
  __ZN14TbSoundSPCodec13SetSampleRateEt   # TbSoundSPCodec::SetSampleRate(unsigned short)
  __ZN14TbSoundSPCodec15GetDataPositionEv   # TbSoundSPCodec::GetDataPosition()
  __ZN14TbSoundSPCodec15SetDataPositionEj   # TbSoundSPCodec::SetDataPosition(unsigned int)
  __ZN14TbSoundSPCodec7GetInfoER17TbSoundSampleInfo   # TbSoundSPCodec::GetInfo(TbSoundSampleInfo&)
  __ZN14TbSoundSPCodec7RestartEv   # TbSoundSPCodec::Restart()
  __ZN14TbSoundSPCodec7SetInfoERK17TbSoundSampleInfo   # TbSoundSPCodec::SetInfo(TbSoundSampleInfo const&)
  __ZN14TbSoundSPCodec8GetCodecEv   # TbSoundSPCodec::GetCodec()
  __ZN14TbSoundSPCodecC1Ev   # TbSoundSPCodec::TbSoundSPCodec()
  __ZN14TbSoundSPCodecD1Ev   # TbSoundSPCodec::~TbSoundSPCodec()
  __ZN15TbSoundDistanceC1Efff   # TbSoundDistance::TbSoundDistance(float, float, float)
  __ZN15TbSoundDistanceC1Ev   # TbSoundDistance::TbSoundDistance()
  __ZN15TbSoundPosition3SetEfff   # TbSoundPosition::Set(float, float, float)
  __ZN15TbSoundPosition3SetEiii   # TbSoundPosition::Set(int, int, int)
  __ZN15TbSoundPosition4SetXEf   # TbSoundPosition::SetX(float)
  __ZN15TbSoundPosition4SetYEf   # TbSoundPosition::SetY(float)
  __ZN15TbSoundPosition4SetZEf   # TbSoundPosition::SetZ(float)
  __ZN15TbSoundPositionC1Eiii   # TbSoundPosition::TbSoundPosition(int, int, int)
  __ZN15TbSoundPositionC1Ev   # TbSoundPosition::TbSoundPosition()
  __ZN15TbSoundPositiondVEf   # TbSoundPosition::operator/=(float)
  __ZN15TbSoundPositionmLEf   # TbSoundPosition::operator*=(float)
  __ZN15TbSoundPositionmiERKS_   # TbSoundPosition::operator-(TbSoundPosition const&)
  __ZN15TbSoundPositionplERKS_   # TbSoundPosition::operator+(TbSoundPosition const&)
  __ZN17TbSoundReverbInfoC1Ev   # TbSoundReverbInfo::TbSoundReverbInfo()
  __ZN17TbSoundSampleInfo11SetCallbackEP13TbSndCallback   # TbSoundSampleInfo::SetCallback(TbSndCallback*)
  __ZN17TbSoundSampleInfo11SetDistanceERK15TbSoundDistance   # TbSoundSampleInfo::SetDistance(TbSoundDistance const&)
  __ZN17TbSoundSampleInfo11SetPositionERK15TbSoundPosition   # TbSoundSampleInfo::SetPosition(TbSoundPosition const&)
  __ZN17TbSoundSampleInfo11SetPriorityEj   # TbSoundSampleInfo::SetPriority(unsigned int)
  __ZN17TbSoundSampleInfo11SetUserDataEPv   # TbSoundSampleInfo::SetUserData(void*)
  __ZN17TbSoundSampleInfo11SetVelocityERK15TbSoundPosition   # TbSoundSampleInfo::SetVelocity(TbSoundPosition const&)
  __ZN17TbSoundSampleInfo12SetFrequencyEf   # TbSoundSampleInfo::SetFrequency(float)
  __ZN17TbSoundSampleInfo12SetLoopCountEj   # TbSoundSampleInfo::SetLoopCount(unsigned int)
  __ZN17TbSoundSampleInfo16SetVolumeControlERK20TbSoundVolumeControlj   # TbSoundSampleInfo::SetVolumeControl(TbSoundVolumeControl const&, unsigned int)
  __ZN17TbSoundSampleInfo20SetCompletedCallbackEP13TbSndCallback   # TbSoundSampleInfo::SetCompletedCallback(TbSndCallback*)
  __ZN17TbSoundSampleInfo22SetStreamingBufferSizeEj   # TbSoundSampleInfo::SetStreamingBufferSize(unsigned int)
  __ZN17TbSoundSampleInfo6SetPanEj   # TbSoundSampleInfo::SetPan(unsigned int)
  __ZN17TbSoundSampleInfo8SetPitchEi   # TbSoundSampleInfo::SetPitch(int)
  __ZN17TbSoundSampleInfo9SetNumberEj   # TbSoundSampleInfo::SetNumber(unsigned int)
  __ZN17TbSoundSampleInfo9SetReverbERK17TbSoundReverbInfo   # TbSoundSampleInfo::SetReverb(TbSoundReverbInfo const&)
  __ZN17TbSoundSampleInfo9SetUserIdEj   # TbSoundSampleInfo::SetUserId(unsigned int)
  __ZN17TbSoundSampleInfo9SetVolumeEj   # TbSoundSampleInfo::SetVolume(unsigned int)
  __ZN17TbSoundSampleInfoC1Ev   # TbSoundSampleInfo::TbSoundSampleInfo()
  __ZN17TbSoundSamplePipe10ClearCacheEv   # TbSoundSamplePipe::ClearCache()
  __ZN17TbSoundSamplePipe12GetDataChunkEPviRib   # TbSoundSamplePipe::GetDataChunk(void*, int, int&, bool)
  __ZN17TbSoundSamplePipe12SetDataStartEPv   # TbSoundSamplePipe::SetDataStart(void*)
  __ZN17TbSoundSamplePipe13AttachReplaceEPS_S0_   # TbSoundSamplePipe::AttachReplace(TbSoundSamplePipe*, TbSoundSamplePipe*)
  __ZN17TbSoundSamplePipe13GetSampleRateEv   # TbSoundSamplePipe::GetSampleRate()
  __ZN17TbSoundSamplePipe21mLowTbSoundSamplePipeE   # TbSoundSamplePipe::mLowTbSoundSamplePipe
  __ZN17TbSoundSamplePipe22mHighTbSoundSamplePipeE   # TbSoundSamplePipe::mHighTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mFirstTbSoundSamplePipeE   # TbSoundSamplePipe::mFirstTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mLevelTbSoundSamplePipeE   # TbSoundSamplePipe::mLevelTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mTotalTbSoundSamplePipeE   # TbSoundSamplePipe::mTotalTbSoundSamplePipe
  __ZN17TbSoundSamplePipe25mCurrentTbSoundSamplePipeE   # TbSoundSamplePipe::mCurrentTbSoundSamplePipe
  __ZN17TbSoundSamplePipe5TopUpEv   # TbSoundSamplePipe::TopUp()
  __ZN17TbSoundSamplePipe7AttatchEPS_   # TbSoundSamplePipe::Attatch(TbSoundSamplePipe*)
  __ZN17TbSoundSamplePipe7DetatchEv   # TbSoundSamplePipe::Detatch()
  __ZN17TbSoundSamplePipe7GetInfoER17TbSoundSampleInfo   # TbSoundSamplePipe::GetInfo(TbSoundSampleInfo&)
  __ZN17TbSoundSamplePipe7ReleaseEv   # TbSoundSamplePipe::Release()
  __ZN17TbSoundSamplePipe7RestartEv   # TbSoundSamplePipe::Restart()
  __ZN17TbSoundSamplePipe7SetInfoERK17TbSoundSampleInfo   # TbSoundSamplePipe::SetInfo(TbSoundSampleInfo const&)
  __ZN17TbSoundSamplePipe8FindPipeEPv   # TbSoundSamplePipe::FindPipe(void*)
  __ZN17TbSoundSamplePipe8SetLevelEi   # TbSoundSamplePipe::SetLevel(int)
  __ZN17TbSoundSamplePipe9CompletedEb   # TbSoundSamplePipe::Completed(bool)
  __ZN17TbSoundSamplePipe9SetHandleEPv   # TbSoundSamplePipe::SetHandle(void*)
  __ZN17TbSoundSamplePipeC1Ev   # TbSoundSamplePipe::TbSoundSamplePipe()
  __ZN17TbSoundSamplePipeD1Ev   # TbSoundSamplePipe::~TbSoundSamplePipe()
  __ZN17TbSoundSamplePipedlEPvm   # TbSoundSamplePipe::operator delete(void*, unsigned long)
  __ZN17TbSoundSamplePipenwEm   # TbSoundSamplePipe::operator new(unsigned long)
  __ZN17TbSoundSystemInfo10SetClassIdEPN10NS_MacDoze5_GUIDE   # TbSoundSystemInfo::SetClassId(NS_MacDoze::_GUID*)
  __ZN17TbSoundSystemInfo11SetHalfRateEj   # TbSoundSystemInfo::SetHalfRate(unsigned int)
  __ZN17TbSoundSystemInfo13SetMaxBuffersEj   # TbSoundSystemInfo::SetMaxBuffers(unsigned int)
  __ZN17TbSoundSystemInfo15SetBuffersToUseEj   # TbSoundSystemInfo::SetBuffersToUse(unsigned int)
  __ZN17TbSoundSystemInfo16SetSpeakerConfigERKN9TbSpeaker6ConfigE   # TbSoundSystemInfo::SetSpeakerConfig(TbSpeaker::Config const&)
  __ZN17TbSoundSystemInfo17SetHardwareDeviceEPv   # TbSoundSystemInfo::SetHardwareDevice(void*)
  __ZN17TbSoundSystemInfo19SetListenerPositionERK15TbSoundPosition   # TbSoundSystemInfo::SetListenerPosition(TbSoundPosition const&)
  __ZN17TbSoundSystemInfo19SetListenerVelocityERK15TbSoundPosition   # TbSoundSystemInfo::SetListenerVelocity(TbSoundPosition const&)
  __ZN17TbSoundSystemInfo20SetListenerDirectionERK15TbSoundPositionS2_   # TbSoundSystemInfo::SetListenerDirection(TbSoundPosition const&, TbSoundPosition const&)
  __ZN17TbSoundSystemInfo7GetMaskEv   # TbSoundSystemInfo::GetMask()
  __ZN17TbSoundSystemInfo7SetGuidEPN10NS_MacDoze5_GUIDE   # TbSoundSystemInfo::SetGuid(NS_MacDoze::_GUID*)
  __ZN17TbSoundSystemInfo7SetHWndEPv   # TbSoundSystemInfo::SetHWnd(void*)
  __ZN17TbSoundSystemInfoC1ERKN9TbSpeaker6ConfigE   # TbSoundSystemInfo::TbSoundSystemInfo(TbSpeaker::Config const&)
  __ZN17TbSoundSystemInfoC1ERKS_   # TbSoundSystemInfo::TbSoundSystemInfo(TbSoundSystemInfo const&)
  __ZN17TbSoundSystemInfoC1Ev   # TbSoundSystemInfo::TbSoundSystemInfo()
  __ZN18TbSoundSampleCodec10GetBitRateEv   # TbSoundSampleCodec::GetBitRate()
  __ZN18TbSoundSampleCodec11GetChannelsEv   # TbSoundSampleCodec::GetChannels()
  __ZN18TbSoundSampleCodec11GetDataSizeEv   # TbSoundSampleCodec::GetDataSize()
  __ZN18TbSoundSampleCodec12GetDataStartEv   # TbSoundSampleCodec::GetDataStart()
  __ZN18TbSoundSampleCodec13GetSampleRateEv   # TbSoundSampleCodec::GetSampleRate()
  __ZN18TbSoundSampleCodec13SetSampleRateEt   # TbSoundSampleCodec::SetSampleRate(unsigned short)
  __ZN18TbSoundSampleCodec14IsTimeCompleteEv   # TbSoundSampleCodec::IsTimeComplete()
  __ZN18TbSoundSampleCodec15GetDataPositionEv   # TbSoundSampleCodec::GetDataPosition()
  __ZN18TbSoundSampleCodec15SetDataPositionEj   # TbSoundSampleCodec::SetDataPosition(unsigned int)
  __ZN18TbSoundSampleCodec25SetSampleFormatFromHeaderEP14TbSampleHeader   # TbSoundSampleCodec::SetSampleFormatFromHeader(TbSampleHeader*)
  __ZN18TbSoundSampleCodec5TimedEv   # TbSoundSampleCodec::Timed()
  __ZN18TbSoundSampleCodec7GetInfoER17TbSoundSampleInfo   # TbSoundSampleCodec::GetInfo(TbSoundSampleInfo&)
  __ZN18TbSoundSampleCodec7SetInfoERK17TbSoundSampleInfo   # TbSoundSampleCodec::SetInfo(TbSoundSampleInfo const&)
  __ZN18TbSoundSampleCodec7SetTimeEbj   # TbSoundSampleCodec::SetTime(bool, unsigned int)
  __ZN18TbSoundSampleCodecC1EP14TbSampleHeaderP12TbRefRelease   # TbSoundSampleCodec::TbSoundSampleCodec(TbSampleHeader*, TbRefRelease*)
  __ZN18TbSoundSampleCodecC1Ev   # TbSoundSampleCodec::TbSoundSampleCodec()
  __ZN18TbSoundSampleCodecD1Ev   # TbSoundSampleCodec::~TbSoundSampleCodec()
  __ZN20TbSoundVolumeControl10SetVolumesEPf   # TbSoundVolumeControl::SetVolumes(float*)
  __ZN20TbSoundVolumeControl4InitEj   # TbSoundVolumeControl::Init(unsigned int)
  __ZN20TbSoundVolumeControl9GetVolumeEj   # TbSoundVolumeControl::GetVolume(unsigned int)
  __ZN20TbSoundVolumeControlC1Ev   # TbSoundVolumeControl::TbSoundVolumeControl()
  __ZNK15TbSoundPosition10DotProductERKS_   # TbSoundPosition::DotProduct(TbSoundPosition const&) const
  __ZNK15TbSoundPosition3GetERfS0_S0_   # TbSoundPosition::Get(float&, float&, float&) const
  __ZNK15TbSoundPosition5ValidEv   # TbSoundPosition::Valid() const
  __ZNK15TbSoundPosition6lengthEv   # TbSoundPosition::length() const
  __ZNK15TbSoundPositionneERKS_   # TbSoundPosition::operator!=(TbSoundPosition const&) const
  __ZNK17TbSoundSystemInfo11GetFrontDirEv   # TbSoundSystemInfo::GetFrontDir() const
  __ZNK17TbSoundSystemInfo13GetMaxBuffersEv   # TbSoundSystemInfo::GetMaxBuffers() const
  __ZNK17TbSoundSystemInfo15GetBuffersToUseEv   # TbSoundSystemInfo::GetBuffersToUse() const
  __ZNK17TbSoundSystemInfo16GetSpeakerConfigEv   # TbSoundSystemInfo::GetSpeakerConfig() const
  __ZNK17TbSoundSystemInfo17GetSamplesPlayingEv   # TbSoundSystemInfo::GetSamplesPlaying() const
  __ZNK17TbSoundSystemInfo19GetListenerPositionEv   # TbSoundSystemInfo::GetListenerPosition() const
  __ZNK17TbSoundSystemInfo19GetListenerVelocityEv   # TbSoundSystemInfo::GetListenerVelocity() const
  __ZNK17TbSoundSystemInfo9GetTopDirEv   # TbSoundSystemInfo::GetTopDir() const
  __ZNK18TbSoundSampleCodec15GetSampleFormatEv   # TbSoundSampleCodec::GetSampleFormat() const
)
set(TPW_UNIT_SoundChannel_OWN_CALLLOG ON)
# The unit file includes src/TbSoundPipeChannel.cpp, src/TbSoundSampleStore.cpp, src/snd_handles.cpp of before the regroup itself; it compiles copies of the files that hold their
# definitions now (TPW_UNIT_SoundChannel_INCLUDED), sliced to those definitions (cmake/slicer.cmake).
set(TPW_UNIT_SoundChannel_INCLUDED src/sound/CSprocketsModule.cpp src/sound/TbSoundCore.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundSampleStore.cpp src/sound/TbSoundSampleWAV.cpp
    src/sound/AudioComponent.cpp src/sound/ChannelState.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCodecLoop.cpp)
set(TPW_UNIT_SoundChannel_INCLUDED_KEEP
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
  __ZN18TbSoundPipeChannel11GetPriorityEv   # TbSoundPipeChannel::GetPriority()
  __ZN18TbSoundPipeChannel11ReleasePipeEb   # TbSoundPipeChannel::ReleasePipe(bool)
  __ZN18TbSoundPipeChannel11SetPipeInfoEPvR17TbSoundSampleInfo   # TbSoundPipeChannel::SetPipeInfo(void*, TbSoundSampleInfo&)
  __ZN18TbSoundPipeChannel11SetPriorityEj   # TbSoundPipeChannel::SetPriority(unsigned int)
  __ZN18TbSoundPipeChannel12GetDataChunkEPviRib   # TbSoundPipeChannel::GetDataChunk(void*, int, int&, bool)
  __ZN18TbSoundPipeChannel13GetNextHandleEv   # TbSoundPipeChannel::GetNextHandle()
  __ZN18TbSoundPipeChannel14GetChannelInfoERjS0_   # TbSoundPipeChannel::GetChannelInfo(unsigned int&, unsigned int&)
  __ZN18TbSoundPipeChannel14GetChannelUsedEv   # TbSoundPipeChannel::GetChannelUsed()
  __ZN18TbSoundPipeChannel14GetSampleFlagsEv   # TbSoundPipeChannel::GetSampleFlags()
  __ZN18TbSoundPipeChannel15GetSampleHeaderEv   # TbSoundPipeChannel::GetSampleHeader()
  __ZN18TbSoundPipeChannel15InitChannelInfoER17TbSoundSampleInfoRj   # TbSoundPipeChannel::InitChannelInfo(TbSoundSampleInfo&, unsigned int&)
  __ZN18TbSoundPipeChannel15SetSampleHeaderERK14TbSampleHeader   # TbSoundPipeChannel::SetSampleHeader(TbSampleHeader const&)
  __ZN18TbSoundPipeChannel16StreamerCallbackEPvjS0_   # TbSoundPipeChannel::StreamerCallback(void*, unsigned int, void*)
  __ZN18TbSoundPipeChannel17FillServiceBufferEv   # TbSoundPipeChannel::FillServiceBuffer()
  __ZN18TbSoundPipeChannel7AttatchEP17TbSoundSamplePipe   # TbSoundPipeChannel::Attatch(TbSoundSamplePipe*)
  __ZN18TbSoundPipeChannel7SetInfoERK17TbSoundSampleInfo   # TbSoundPipeChannel::SetInfo(TbSoundSampleInfo const&)
  __ZN18TbSoundPipeChannel8FindPipeERK13CUniqueHandle   # TbSoundPipeChannel::FindPipe(CUniqueHandle const&)
  __ZN18TbSoundPipeChannel8GetIndexEv   # TbSoundPipeChannel::GetIndex()
  __ZN18TbSoundPipeChannel8SetIndexEt   # TbSoundPipeChannel::SetIndex(unsigned short)
  __ZN18TbSoundPipeChannel9GetHandleEv   # TbSoundPipeChannel::GetHandle()
  __ZN18TbSoundPipeChannel9GetVolumeEv   # TbSoundPipeChannel::GetVolume()
  __ZN18TbSoundPipeChannel9SetVolumeEj   # TbSoundPipeChannel::SetVolume(unsigned int)
  __ZN18TbSoundPipeChannelC1Ev   # TbSoundPipeChannel::TbSoundPipeChannel()
  __ZN18TbSoundPipeChannelD1Ev   # TbSoundPipeChannel::~TbSoundPipeChannel()
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
  __ZNK18TbSoundPipeChannel12ControlledByEj   # TbSoundPipeChannel::ControlledBy(unsigned int) const
  __ZNK18TbSoundPipeChannel15GetActualVolumeEv   # TbSoundPipeChannel::GetActualVolume() const
  __ZNK18TbSoundSampleStoreixERK13CUniqueHandle   # TbSoundSampleStore::operator[](CUniqueHandle const&) const
  __ZNK18TbSoundSampleStoreixEj   # TbSoundSampleStore::operator[](unsigned int) const
)
