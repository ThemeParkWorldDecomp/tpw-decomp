# Unit w79d: the SystemCommand Execute bodies of the sound library (src/sound/SystemCommand.cpp). Real: the command constructors
# (w79b), the info records and positions (w77), the device-module base and the handle values (w79a), the pipe base (w78) and
# TbRefRelease. The unit file fakes the access object's core call, the device module, the sample store and its iterator, the
# channels and SamsUtilities::UDebug.
set(TPW_UNIT_SoundCommands src/sound/SystemCommand.cpp src/sound/AudioPlaceHolder.cpp src/sound/TbSoundCore.cpp
    src/sound/TbSysCommand.cpp src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundManager.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp
    src/sound/TbSoundSampleStore.cpp src/sound/TbSoundSystemInfo.cpp src/sound/ServiceQueue.cpp
    src/sound/TbSoundSystemModule.cpp src/sound/AudioComponent.cpp src/sound/ChannelState.cpp
    src/sound/TbSoundSampleWAV.cpp src/bullfrog/TbRefRelease.cpp src/sound/StreamableMpeg2.cpp
    src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleCodec.cpp src/sound/TbSoundSamplePipe.cpp)
set(TPW_UNIT_SoundCommands_KEEP
  __ZN12CAtomicValueC1Ev   # CAtomicValue::CAtomicValue()
  __ZN12CAtomicValuemmEv   # CAtomicValue::operator--()
  __ZN12CAtomicValueppEv   # CAtomicValue::operator++()
  __ZN12TbRefRelease6AddRefEv   # TbRefRelease::AddRef()
  __ZN12TbRefRelease7ReleaseEv   # TbRefRelease::Release()
  __ZN12TbRefReleaseC1Ev   # TbRefRelease::TbRefRelease()
  __ZN12TbRefReleaseD1Ev   # TbRefRelease::~TbRefRelease()
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
  __ZN13SystemCommand10RestartAllC1Ev   # SystemCommand::RestartAll::RestartAll()
  __ZN13SystemCommand13HandleCommandC1EPv   # SystemCommand::HandleCommand::HandleCommand(void*)
  __ZN13SystemCommand13SetSampleInfoC1EPvR17TbSoundSampleInfo   # SystemCommand::SetSampleInfo::SetSampleInfo(void*, TbSoundSampleInfo&)
  __ZN13SystemCommand13SetSystemInfoC1ER17TbSoundSystemInfo   # SystemCommand::SetSystemInfo::SetSystemInfo(TbSoundSystemInfo&)
  __ZN13SystemCommand16UpdateAllVolumesC1Ev   # SystemCommand::UpdateAllVolumes::UpdateAllVolumes()
  __ZN13SystemCommand16sStartInProgressE   # SystemCommand::sStartInProgress
  __ZN13SystemCommand17sStartInitialisedE   # SystemCommand::sStartInitialised
  __ZN13SystemCommand4StopC1EPv   # SystemCommand::Stop::Stop(void*)
  __ZN13SystemCommand5PauseC1EPv   # SystemCommand::Pause::Pause(void*)
  __ZN13SystemCommand5StartC1EPv   # SystemCommand::Start::Start(void*)
  __ZN13SystemCommand7CommandC1Ev   # SystemCommand::Command::Command()
  __ZN13SystemCommand7ProcessC1Ev   # SystemCommand::Process::Process()
  __ZN13SystemCommand7RestartC1EPv   # SystemCommand::Restart::Restart(void*)
  __ZN13SystemCommand7StopAllC1Ev   # SystemCommand::StopAll::StopAll()
  __ZN13SystemCommand8PauseAllC1Ev   # SystemCommand::PauseAll::PauseAll()
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
  __ZN15CAudioComponent10NewControlEPvR17TbSoundSampleInfo   # CAudioComponent::NewControl(void*, TbSoundSampleInfo&)
  __ZN15CAudioComponent12GetComponentEj   # CAudioComponent::GetComponent(unsigned int)
  __ZN15CAudioComponent13GetSampleInfoEPvR17TbSoundSampleInfo   # CAudioComponent::GetSampleInfo(void*, TbSoundSampleInfo&)
  __ZN15CAudioComponent13GetSystemInfoER17TbSoundSystemInfo   # CAudioComponent::GetSystemInfo(TbSoundSystemInfo&)
  __ZN15CAudioComponent13SystemCommandERKN13SystemCommand7CommandE   # CAudioComponent::SystemCommand(SystemCommand::Command const&)
  __ZN15CAudioComponent17UnchainAndReleaseEv   # CAudioComponent::UnchainAndRelease()
  __ZN15CAudioComponent4InitERK17TbSoundSystemInfo   # CAudioComponent::Init(TbSoundSystemInfo const&)
  __ZN15CAudioComponent5ChainEPS_   # CAudioComponent::Chain(CAudioComponent*)
  __ZN15CAudioComponent9NewSampleEPvR17TbSoundSampleInfo   # CAudioComponent::NewSample(void*, TbSoundSampleInfo&)
  __ZN15CAudioComponentC1Ev   # CAudioComponent::CAudioComponent()
  __ZN15CAudioComponentD1Ev   # CAudioComponent::~CAudioComponent()
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
  __ZN19TbSoundSystemModule10RestartAllEv   # TbSoundSystemModule::RestartAll()
  __ZN19TbSoundSystemModule10SetOptionsEj   # TbSoundSystemModule::SetOptions(unsigned int)
  __ZN19TbSoundSystemModule10SetPanRateEi   # TbSoundSystemModule::SetPanRate(int)
  __ZN19TbSoundSystemModule11GetPositionEjR15TbSoundPosition   # TbSoundSystemModule::GetPosition(unsigned int, TbSoundPosition&)
  __ZN19TbSoundSystemModule11GetVelocityEjR15TbSoundPosition   # TbSoundSystemModule::GetVelocity(unsigned int, TbSoundPosition&)
  __ZN19TbSoundSystemModule11SetPositionEjRK15TbSoundPositioni   # TbSoundSystemModule::SetPosition(unsigned int, TbSoundPosition const&, int)
  __ZN19TbSoundSystemModule11SetVelocityEjRK15TbSoundPosition   # TbSoundSystemModule::SetVelocity(unsigned int, TbSoundPosition const&)
  __ZN19TbSoundSystemModule12PauseChannelEj   # TbSoundSystemModule::PauseChannel(unsigned int)
  __ZN19TbSoundSystemModule12SetFrequencyEjjf   # TbSoundSystemModule::SetFrequency(unsigned int, unsigned int, float)
  __ZN19TbSoundSystemModule13GetBufferSizeEP18TbSoundPipeChannel   # TbSoundSystemModule::GetBufferSize(TbSoundPipeChannel*)
  __ZN19TbSoundSystemModule14RestartChannelEj   # TbSoundSystemModule::RestartChannel(unsigned int)
  __ZN19TbSoundSystemModule14SetChannelInfoEjP18TbSoundPipeChannelR17TbSoundSampleInfo   # TbSoundSystemModule::SetChannelInfo(unsigned int, TbSoundPipeChannel*, TbSoundSampleInfo&)
  __ZN19TbSoundSystemModule15GetSystemObjectEj   # TbSoundSystemModule::GetSystemObject(unsigned int)
  __ZN19TbSoundSystemModule15SetSpeedOfSoundEf   # TbSoundSystemModule::SetSpeedOfSound(float)
  __ZN19TbSoundSystemModule16IsChannelPlayingEj   # TbSoundSystemModule::IsChannelPlaying(unsigned int)
  __ZN19TbSoundSystemModule16SetSpeakerConfigERKN9TbSpeaker6ConfigE   # TbSoundSystemModule::SetSpeakerConfig(TbSpeaker::Config const&)
  __ZN19TbSoundSystemModule17SetDistanceFactorEjRK15TbSoundDistance   # TbSoundSystemModule::SetDistanceFactor(unsigned int, TbSoundDistance const&)
  __ZN19TbSoundSystemModule17SetReverbPositionERK15TbSoundPosition   # TbSoundSystemModule::SetReverbPosition(TbSoundPosition const&)
  __ZN19TbSoundSystemModule19SetListenerPositionERK15TbSoundPosition   # TbSoundSystemModule::SetListenerPosition(TbSoundPosition const&)
  __ZN19TbSoundSystemModule19SetListenerVelocityERK15TbSoundPosition   # TbSoundSystemModule::SetListenerVelocity(TbSoundPosition const&)
  __ZN19TbSoundSystemModule20GetDirectSoundBufferEjRP18IDirectSoundBuffer   # TbSoundSystemModule::GetDirectSoundBuffer(unsigned int, IDirectSoundBuffer*&)
  __ZN19TbSoundSystemModule20GetDirectSoundObjectERP12IDirectSound   # TbSoundSystemModule::GetDirectSoundObject(IDirectSound*&)
  __ZN19TbSoundSystemModule20SetListenerDirectionERK15TbSoundPositionS2_   # TbSoundSystemModule::SetListenerDirection(TbSoundPosition const&, TbSoundPosition const&)
  __ZN19TbSoundSystemModule20SetReverbEnvironmentEjfff   # TbSoundSystemModule::SetReverbEnvironment(unsigned int, float, float, float)
  __ZN19TbSoundSystemModule21Set3DDSPFunctionalityEjj   # TbSoundSystemModule::Set3DDSPFunctionality(unsigned int, unsigned int)
  __ZN19TbSoundSystemModule28ConvertPitchIndexToFrequencyEi   # TbSoundSystemModule::ConvertPitchIndexToFrequency(int)
  __ZN19TbSoundSystemModule28SupportsEnvironmentalEffectsEv   # TbSoundSystemModule::SupportsEnvironmentalEffects()
  __ZN19TbSoundSystemModule4InitERK17TbSoundSystemInfo   # TbSoundSystemModule::Init(TbSoundSystemInfo const&)
  __ZN19TbSoundSystemModule7ProcessEv   # TbSoundSystemModule::Process()
  __ZN19TbSoundSystemModule7StopAllEv   # TbSoundSystemModule::StopAll()
  __ZN19TbSoundSystemModule8PauseAllEv   # TbSoundSystemModule::PauseAll()
  __ZN19TbSoundSystemModule8SetPitchEjji   # TbSoundSystemModule::SetPitch(unsigned int, unsigned int, int)
  __ZN19TbSoundSystemModule9SetLoPassEjf   # TbSoundSystemModule::SetLoPass(unsigned int, float)
  __ZN19TbSoundSystemModuleC1Ev   # TbSoundSystemModule::TbSoundSystemModule()
  __ZN19TbSoundSystemModuleD1Ev   # TbSoundSystemModule::~TbSoundSystemModule()
  __ZN20CAutoCriticalSectionC1Ev   # CAutoCriticalSection::CAutoCriticalSection()
  __ZN20CAutoCriticalSectionD1Ev   # CAutoCriticalSection::~CAutoCriticalSection()
  __ZN20CAutoCriticalSectioncvPPvEv   # CAutoCriticalSection::operator void**()
  __ZN20TbSoundVolumeControl10SetVolumesEPf   # TbSoundVolumeControl::SetVolumes(float*)
  __ZN20TbSoundVolumeControl4InitEj   # TbSoundVolumeControl::Init(unsigned int)
  __ZN20TbSoundVolumeControl9GetVolumeEj   # TbSoundVolumeControl::GetVolume(unsigned int)
  __ZN20TbSoundVolumeControlC1Ev   # TbSoundVolumeControl::TbSoundVolumeControl()
  __ZN21CAudioComponentAccessC1Ev   # CAudioComponentAccess::CAudioComponentAccess()
  __ZN22TbSoundCriticalSectionC1EPv   # TbSoundCriticalSection::TbSoundCriticalSection(void*)
  __ZN22TbSoundCriticalSectionD1Ev   # TbSoundCriticalSection::~TbSoundCriticalSection()
  __ZN7CLowestC1Ev   # CLowest::CLowest()
  __ZN7CLowestclEjP18TbSoundPipeChannel   # CLowest::operator()(unsigned int, TbSoundPipeChannel*)
  __ZN9TbSpeaker10AutoDelete10_mpSpeakerE   # TbSpeaker::AutoDelete::_mpSpeaker
  __ZN9TbSpeaker10AutoDeleteD1Ev   # TbSpeaker::AutoDelete::~AutoDelete()
  __ZNK12CAtomicValuecviEv   # CAtomicValue::operator int() const
  __ZNK13CUniqueHandle8GetIndexEv   # CUniqueHandle::GetIndex() const
  __ZNK13CUniqueHandlecvPvEv   # CUniqueHandle::operator void*() const
  __ZNK13SystemCommand10RestartAll7ExecuteEP21CAudioComponentAccess   # SystemCommand::RestartAll::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand13SetSampleInfo14SetChannelInfoEP21CAudioComponentAccessj   # SystemCommand::SetSampleInfo::SetChannelInfo(CAudioComponentAccess*, unsigned int) const
  __ZNK13SystemCommand13SetSampleInfo7ExecuteEP21CAudioComponentAccess   # SystemCommand::SetSampleInfo::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand13SetSystemInfo7ExecuteEP21CAudioComponentAccess   # SystemCommand::SetSystemInfo::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand16UpdateAllVolumes7ExecuteEP21CAudioComponentAccess   # SystemCommand::UpdateAllVolumes::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand20StopAllVolumeControl7ExecuteEP21CAudioComponentAccess   # SystemCommand::StopAllVolumeControl::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand4Stop7ExecuteEP21CAudioComponentAccess   # SystemCommand::Stop::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand5Pause7ExecuteEP21CAudioComponentAccess   # SystemCommand::Pause::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand5Start7ExecuteEP21CAudioComponentAccess   # SystemCommand::Start::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand7Process7ExecuteEP21CAudioComponentAccess   # SystemCommand::Process::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand7Restart7ExecuteEP21CAudioComponentAccess   # SystemCommand::Restart::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand7StopAll7ExecuteEP21CAudioComponentAccess   # SystemCommand::StopAll::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand8PauseAll7ExecuteEP21CAudioComponentAccess   # SystemCommand::PauseAll::Execute(CAudioComponentAccess*) const
  __ZNK13SystemCommand8StopLoop7ExecuteEP21CAudioComponentAccess   # SystemCommand::StopLoop::Execute(CAudioComponentAccess*) const
  __ZNK15CAudioComponent14GetSampleStoreEv   # CAudioComponent::GetSampleStore() const
  __ZNK15CAudioComponent15GetSystemModuleEv   # CAudioComponent::GetSystemModule() const
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
