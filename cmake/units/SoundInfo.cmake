# Unit w77: the sound library's value classes (TbSoundPosition, TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl,
# TbSoundSampleInfo and TbSoundSystemInfo). Self-contained: no callees outside the unit on the host.
set(TPW_UNIT_SoundInfo src/sound/AudioPlaceHolder.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCodecLoop.cpp
    src/sound/TbSoundCore.cpp src/sound/TbSoundManager.cpp src/sound/TbSoundPipe.cpp src/sound/TbSoundPipeChannel.cpp
    src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp src/sound/TbSoundSampleStore.cpp
    src/sound/TbSoundSystemInfo.cpp src/sound/TbSysCommand.cpp)
set(TPW_UNIT_SoundInfo_KEEP
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
