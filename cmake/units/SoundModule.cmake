# w79a: device-module base and channel values. The shim includes the translated sources
# so the external TbError constructor and powf can be recorded on both sides.
set(TPW_UNIT_SoundModule src/sound/AudioPlaceHolder.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCodecLoop.cpp
    src/sound/TbSoundCore.cpp src/sound/TbSoundManager.cpp src/sound/TbSoundPipe.cpp src/sound/TbSoundPipeChannel.cpp
    src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp src/sound/TbSoundSampleStore.cpp
    src/sound/TbSoundSystemInfo.cpp src/sound/TbSysCommand.cpp)
set(TPW_UNIT_SoundModule_KEEP
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
)
set(TPW_UNIT_SoundModule_OWN_CALLLOG ON)
# The unit file includes src/TbSoundSystemModule.cpp, src/snd_handles.cpp of before the regroup itself; it compiles copies of the files that hold their
# definitions now (TPW_UNIT_SoundModule_INCLUDED), sliced to those definitions (cmake/slicer.cmake).
set(TPW_UNIT_SoundModule_INCLUDED src/sound/ServiceQueue.cpp src/sound/TbSoundCore.cpp
    src/sound/TbSoundSystemModule.cpp src/sound/AudioComponent.cpp src/sound/ChannelState.cpp
    src/sound/SystemCommand.cpp src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundSampleStore.cpp src/sound/TbSoundSampleWAV.cpp)
set(TPW_UNIT_SoundModule_INCLUDED_KEEP
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
  __ZN22TbSoundCriticalSectionC1EPv   # TbSoundCriticalSection::TbSoundCriticalSection(void*)
  __ZN22TbSoundCriticalSectionD1Ev   # TbSoundCriticalSection::~TbSoundCriticalSection()
  __ZN7CLowestC1Ev   # CLowest::CLowest()
  __ZN7CLowestclEjP18TbSoundPipeChannel   # CLowest::operator()(unsigned int, TbSoundPipeChannel*)
  __ZN9TbSpeaker10AutoDelete10_mpSpeakerE   # TbSpeaker::AutoDelete::_mpSpeaker
  __ZN9TbSpeaker10AutoDeleteD1Ev   # TbSpeaker::AutoDelete::~AutoDelete()
  __ZNK12CAtomicValuecviEv   # CAtomicValue::operator int() const
  __ZNK13CUniqueHandle8GetIndexEv   # CUniqueHandle::GetIndex() const
  __ZNK13CUniqueHandlecvPvEv   # CUniqueHandle::operator void*() const
)
