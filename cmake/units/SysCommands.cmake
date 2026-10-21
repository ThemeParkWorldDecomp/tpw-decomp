# Unit w79h: the Execute bodies of the sound library's system commands (the 47 TbSysCommand classes) and the five payload accessors of
# their base classes, run against the real audio object of w79f (TbAudioBody: GetSoundManager, GetPlaceHolderList, SystemCommand,
# with two of its virtuals hooked), the info and sample-info records (w77) and the library's own command constructors (w79b).
# Fakes in the unit file, each recording what the original's callee is given: the sound manager and the core under it, the
# placeholder list and a placeholder (w79g1, w79g2), TpBody::Tick/Save/Load (untranslated), CvtDblUl and the streamer calls the
# body links.
set(TPW_UNIT_SysCommands src/sound/TbSysCommand.cpp src/sound/TpBody.cpp src/sound/AudioComponent.cpp
    src/sound/TbAudioBody.cpp src/sound/AudioPlaceHolder.cpp src/sound/TbSoundCore.cpp src/sound/SystemCommand.cpp
    src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundManager.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp
    src/sound/TbSoundSampleStore.cpp src/sound/TbSoundSystemInfo.cpp src/bullfrog/TbRefRelease.cpp
    src/bullfrog/FileSystem.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_SysCommands_KEEP
  __Z7IsSlashc   # IsSlash(char)
  __ZN11TbAudioBody11QualifyPathEPKcR23TbDynamicStringTemplateIcE   # TbAudioBody::QualifyPath(char const*, TbDynamicStringTemplate<char>&)
  __ZN11TbAudioBody12UpdateHandleEiRK15TbSoundPositionS2_   # TbAudioBody::UpdateHandle(int, TbSoundPosition const&, TbSoundPosition const&)
  __ZN11TbAudioBody13UpdateLowPassEPvf   # TbAudioBody::UpdateLowPass(void*, float)
  __ZN11TbAudioBody15UpdateParameterEihj   # TbAudioBody::UpdateParameter(int, unsigned char, unsigned int)
  __ZN11TbAudioBody16RegisterCatagoryEPKcS1_Rj   # TbAudioBody::RegisterCatagory(char const*, char const*, unsigned int&)
  __ZN11TbAudioBody16UpdateDistortionEPvf   # TbAudioBody::UpdateDistortion(void*, float)
  __ZN11TbAudioBody17CreateSystemChainERK12CAudioConfig   # TbAudioBody::CreateSystemChain(CAudioConfig const&)
  __ZN11TbAudioBody22SetListenerOrientationERK15TbSoundPositionS2_S2_S2_   # TbAudioBody::SetListenerOrientation(TbSoundPosition const&, TbSoundPosition const&, TbSoundPosition const&, TbSoundPosition const&)
  __ZN11TbAudioBody27CanBeDroppedDueToVolControlEj   # TbAudioBody::CanBeDroppedDueToVolControl(unsigned int)
  __ZN11TbAudioBody4InitEPKcRK12CAudioConfig   # TbAudioBody::Init(char const*, CAudioConfig const&)
  __ZN11TbAudioBody7ReleaseEv   # TbAudioBody::Release()
  __ZN11TbAudioBody9SendEventEiRK12TbAudioEvent   # TbAudioBody::SendEvent(int, TbAudioEvent const&)
  __ZN11TbAudioBodyC1Ev   # TbAudioBody::TbAudioBody()
  __ZN11TbAudioBodyD1Ev   # TbAudioBody::~TbAudioBody()
  __ZN12TbRefRelease6AddRefEv   # TbRefRelease::AddRef()
  __ZN12TbRefRelease7ReleaseEv   # TbRefRelease::Release()
  __ZN12TbRefReleaseC1Ev   # TbRefRelease::TbRefRelease()
  __ZN12TbRefReleaseD1Ev   # TbRefRelease::~TbRefRelease()
  __ZN13SystemCommand10RestartAllC1Ev   # SystemCommand::RestartAll::RestartAll()
  __ZN13SystemCommand13HandleCommandC1EPv   # SystemCommand::HandleCommand::HandleCommand(void*)
  __ZN13SystemCommand13SetSampleInfoC1EPvR17TbSoundSampleInfo   # SystemCommand::SetSampleInfo::SetSampleInfo(void*, TbSoundSampleInfo&)
  __ZN13SystemCommand13SetSystemInfoC1ER17TbSoundSystemInfo   # SystemCommand::SetSystemInfo::SetSystemInfo(TbSoundSystemInfo&)
  __ZN13SystemCommand16UpdateAllVolumesC1Ev   # SystemCommand::UpdateAllVolumes::UpdateAllVolumes()
  __ZN13SystemCommand4StopC1EPv   # SystemCommand::Stop::Stop(void*)
  __ZN13SystemCommand5PauseC1EPv   # SystemCommand::Pause::Pause(void*)
  __ZN13SystemCommand5StartC1EPv   # SystemCommand::Start::Start(void*)
  __ZN13SystemCommand7CommandC1Ev   # SystemCommand::Command::Command()
  __ZN13SystemCommand7ProcessC1Ev   # SystemCommand::Process::Process()
  __ZN13SystemCommand7RestartC1EPv   # SystemCommand::Restart::Restart(void*)
  __ZN13SystemCommand7StopAllC1Ev   # SystemCommand::StopAll::StopAll()
  __ZN13SystemCommand8PauseAllC1Ev   # SystemCommand::PauseAll::PauseAll()
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
  __ZN21CAudioComponentAccessC1Ev   # CAudioComponentAccess::CAudioComponentAccess()
  __ZN22CAudioComponentFactory10CreateCoreEv   # CAudioComponentFactory::CreateCore()
  __ZN22CAudioComponentFactory15CreateComponentEv   # CAudioComponentFactory::CreateComponent()
  __ZNK11TbAudioBody10StopHandleEPv   # TbAudioBody::StopHandle(void*) const
  __ZNK11TbAudioBody13SystemCommandERKN12TbSysCommand7CommandE   # TbAudioBody::SystemCommand(TbSysCommand::Command const&) const
  __ZNK11TbAudioBody15GetSoundManagerEv   # TbAudioBody::GetSoundManager() const
  __ZNK11TbAudioBody18GetPlaceHolderListEv   # TbAudioBody::GetPlaceHolderList() const
  __ZNK11TbAudioBody23DeregisterCatagoryBanksEj   # TbAudioBody::DeregisterCatagoryBanks(unsigned int) const
  __ZNK12TbSysCommand10RestartAll7ExecuteEPK11TbAudioBody   # TbSysCommand::RestartAll::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand10StopHandle7ExecuteEPK11TbAudioBody   # TbSysCommand::StopHandle::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand11PauseHandle7ExecuteEPK11TbAudioBody   # TbSysCommand::PauseHandle::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand11SetHalfRate7ExecuteEPK11TbAudioBody   # TbSysCommand::SetHalfRate::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand11ToggleMusic7ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleMusic::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand11ToggleUser17ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleUser1::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand11ToggleUser27ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleUser2::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12DeleteObject7ExecuteEPK11TbAudioBody   # TbSysCommand::DeleteObject::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12GetSFXVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::GetSFXVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12SetDroppable7ExecuteEPK11TbAudioBody   # TbSysCommand::SetDroppable::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12SetSFXVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetSFXVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12ToggleMaster7ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleMaster::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand12ToggleSpeech7ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleSpeech::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand13FlushCatagory7ExecuteEPK11TbAudioBody   # TbSysCommand::FlushCatagory::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand13RestartHandle7ExecuteEPK11TbAudioBody   # TbSysCommand::RestartHandle::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand13SetQSoundInfo7ExecuteEPK11TbAudioBody   # TbSysCommand::SetQSoundInfo::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand14GetMusicVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::GetMusicVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand14RegisterObject7ExecuteEPK11TbAudioBody   # TbSysCommand::RegisterObject::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand14SetMusicVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetMusicVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand14SetUser1Volume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetUser1Volume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand14SetUser2Volume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetUser2Volume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15GetSampleLength7ExecuteEPK11TbAudioBody   # TbSysCommand::GetSampleLength::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15SetGeometryInfo7ExecuteEPK11TbAudioBody   # TbSysCommand::SetGeometryInfo::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15SetHandleVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetHandleVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15SetMasterVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetMasterVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15SetPresetReverb7ExecuteEPK11TbAudioBody   # TbSysCommand::SetPresetReverb::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15SetSpeechVolume7ExecuteEPK11TbAudioBody   # TbSysCommand::SetSpeechVolume::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand15StopAllWithFade7ExecuteEPK11TbAudioBody   # TbSysCommand::StopAllWithFade::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand16DeleteAllObjects7ExecuteEPK11TbAudioBody   # TbSysCommand::DeleteAllObjects::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand16SetSpeakerConfig7ExecuteEPK11TbAudioBody   # TbSysCommand::SetSpeakerConfig::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand18SetDistanceMapping7ExecuteEPK11TbAudioBody   # TbSysCommand::SetDistanceMapping::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand18StopHandleWithFade7ExecuteEPK11TbAudioBody   # TbSysCommand::StopHandleWithFade::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand19SetNumberOfChannels7ExecuteEPK11TbAudioBody   # TbSysCommand::SetNumberOfChannels::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand20GetDirectSoundObject7ExecuteEPK11TbAudioBody   # TbSysCommand::GetDirectSoundObject::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand20SetBoundingRectangle7ExecuteEPK11TbAudioBody   # TbSysCommand::SetBoundingRectangle::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand22GetMaxNumberOfChannels7ExecuteEPK11TbAudioBody   # TbSysCommand::GetMaxNumberOfChannels::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand23SetHandleVolumeAndPitch7ExecuteEPK11TbAudioBody   # TbSysCommand::SetHandleVolumeAndPitch::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand26GetNumberOfPlayingChannels7ExecuteEPK11TbAudioBody   # TbSysCommand::GetNumberOfPlayingChannels::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand4Load7ExecuteEPK11TbAudioBody   # TbSysCommand::Load::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand4Save7ExecuteEPK11TbAudioBody   # TbSysCommand::Save::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand4Tick7ExecuteEPK11TbAudioBody   # TbSysCommand::Tick::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand6SetEAX7ExecuteEPK11TbAudioBody   # TbSysCommand::SetEAX::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand7Process7ExecuteEPK11TbAudioBody   # TbSysCommand::Process::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand7StopAll7ExecuteEPK11TbAudioBody   # TbSysCommand::StopAll::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand8GetValue11GetSysValueEv   # TbSysCommand::GetValue::GetSysValue() const
  __ZNK12TbSysCommand8PauseAll7ExecuteEPK11TbAudioBody   # TbSysCommand::PauseAll::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand8SetFades7ExecuteEPK11TbAudioBody   # TbSysCommand::SetFades::Execute(TbAudioBody const*) const
  __ZNK12TbSysCommand8SetValue11GetSysValueEv   # TbSysCommand::SetValue::GetSysValue() const
  __ZNK12TbSysCommand8SetValue16GetFloatSysValueEv   # TbSysCommand::SetValue::GetFloatSysValue() const
  __ZNK12TbSysCommand8SetValue19GetTbHandleSysValueEv   # TbSysCommand::SetValue::GetTbHandleSysValue() const
  __ZNK12TbSysCommand9Serialise9GetHandleEv   # TbSysCommand::Serialise::GetHandle() const
  __ZNK12TbSysCommand9ToggleSFX7ExecuteEPK11TbAudioBody   # TbSysCommand::ToggleSFX::Execute(TbAudioBody const*) const
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
  __ZN22TbStaticStringTemplateIcE8ResizeToEj   # template void TbStaticStringTemplate<char>::ResizeTo(uint32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEC1EPcji   # template TbStaticStringTemplate<char>::TbStaticStringTemplate(char*, uint32_t, int32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcED1Ev   # template TbStaticStringTemplate<char>::~TbStaticStringTemplate() (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char&) (explicit instantiation)
  __ZTS12TbStringBaseIDsE   # template class TbStringBase<char16_t> (explicit instantiation)
  __ZTS12TbStringBaseIcE   # template class TbStringBase<char> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIDsE   # template class TbDynamicStringTemplate<char16_t> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIcE   # template class TbDynamicStringTemplate<char> (explicit instantiation)
  tmpl_10f1a43abff8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_12b48e599676@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_13aaac678a90@bullfrog/FileSystem.cpp   # ~TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_156c9453e350@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_177fef366926@bullfrog/FileSystem.cpp   # SBCstrcpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_1b2c80ece410@bullfrog/FileSystem.cpp   # FindOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_1eedb09deef3@bullfrog/FileSystem.cpp   # SBCstrncat (a template definition src/TbString.cpp had before the regroup)
  tmpl_1fb948d1a8d7@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_31520eb2cde0@bullfrog/FileSystem.cpp   # operator< (a template definition src/TbString.cpp had before the regroup)
  tmpl_3b898e0ec42d@bullfrog/FileSystem.cpp   # Length (a template definition src/TbString.cpp had before the regroup)
  tmpl_3f76b5cf6898@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_4209e9eb338a@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_444e69a72b25@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_47746be482c6@bullfrog/FileSystem.cpp   # Right (a template definition src/TbString.cpp had before the regroup)
  tmpl_5072774eb760@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_56b8c8679dbb@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_58045e5a0a73@bullfrog/FileSystem.cpp   # AllocBuffer (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_5e6789bb3ff3@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_62d628fca008@bullfrog/FileSystem.cpp   # FindReverseOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_67728c1f6bc2@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_6fee3b71ae69@bullfrog/FileSystem.cpp   # LockBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_7a81918ce0ed@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_7d7b5671d9c8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_7f7c954a7476@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_8078b9a106a6@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_89c56dab9bda@bullfrog/FileSystem.cpp   # Left (a template definition src/TbString.cpp had before the regroup)
  tmpl_90c753af4ad8@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_923c787ed96b@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_9329dea9cdff@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_966b985e3b04@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_a2ea62013ee7@bullfrog/FileSystem.cpp   # SBCstrlen (a template definition src/TbString.cpp had before the regroup)
  tmpl_a65764511fd4@bullfrog/FileSystem.cpp   # ReleaseBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_aa572ba6c8bb@bullfrog/FileSystem.cpp   # ~TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_b04e950cbb16@bullfrog/FileSystem.cpp   # FindReverse (a template definition src/TbString.cpp had before the regroup)
  tmpl_be6e319ad7a7@bullfrog/FileSystem.cpp   # TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_bf7094367fc1@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_c2f8bf00de94@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_c4b2c4a972d6@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_cd6077897b68@bullfrog/FileSystem.cpp   # operator!= (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3aef466ce77@bullfrog/FileSystem.cpp   # SBCstrcat (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3d969d3322f@bullfrog/FileSystem.cpp   # SBCstrncpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_d4a4fb92aa5f@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_d809f9651e41@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_feba63a511e0@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
)
