# Unit w79g1: CAudioPlaceHolderList, the pool and the priority list of the sound placeholders. Real: the sound lock and the
# critical section (src/sound/AudioComponent.cpp, src/sound/ChannelState.cpp, src/sound/SystemCommand.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TbSoundSampleWAV.cpp), the info and position records (w77: TbSoundInfo.cpp, with TbRefRelease).
# The unit file fakes the placeholder's own members the list calls (CAudioPlaceHolder is NOT linked: its heavy members need the
# sound manager, the map data and the file helpers, and w79g2's test covers them against the original), the nine subclasses'
# CreateObject statics (unit w79g3), the streamer's Instance and fix-up table calls, the clock and the file object. The unit file
# includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_AudioPlaceHolderList src/sound/AudioPlaceHolderList.cpp src/sound/PlaceHolderTypes.cpp
    src/sound/TbSysCommand.cpp src/sound/AudioComponent.cpp src/sound/ChannelState.cpp src/sound/SystemCommand.cpp
    src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundCore.cpp src/sound/TbSoundPipe.cpp src/sound/TbSoundPipeChannel.cpp
    src/sound/TbSoundSampleStore.cpp src/sound/TbSoundSampleWAV.cpp src/sound/AudioPlaceHolder.cpp
    src/sound/TbSoundManager.cpp src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp
    src/sound/TbSoundSystemInfo.cpp src/bullfrog/TbRefRelease.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_AudioPlaceHolderList_KEEP
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
  __ZN20CAutoCriticalSectionC1Ev   # CAutoCriticalSection::CAutoCriticalSection()
  __ZN20CAutoCriticalSectionD1Ev   # CAutoCriticalSection::~CAutoCriticalSection()
  __ZN20CAutoCriticalSectioncvPPvEv   # CAutoCriticalSection::operator void**()
  __ZN20TbSoundVolumeControl10SetVolumesEPf   # TbSoundVolumeControl::SetVolumes(float*)
  __ZN20TbSoundVolumeControl4InitEj   # TbSoundVolumeControl::Init(unsigned int)
  __ZN20TbSoundVolumeControl9GetVolumeEj   # TbSoundVolumeControl::GetVolume(unsigned int)
  __ZN20TbSoundVolumeControlC1Ev   # TbSoundVolumeControl::TbSoundVolumeControl()
  __ZN21CAudioPlaceHolderList10Handle2PtrEPv   # CAudioPlaceHolderList::Handle2Ptr(void*)
  __ZN21CAudioPlaceHolderList11AddNewBlockEv   # CAudioPlaceHolderList::AddNewBlock()
  __ZN21CAudioPlaceHolderList12CreateHandleEP17CAudioPlaceHolder   # CAudioPlaceHolderList::CreateHandle(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList12FindFreeItemEPv   # CAudioPlaceHolderList::FindFreeItem(void*)
  __ZN21CAudioPlaceHolderList14NewHandleIndexEv   # CAudioPlaceHolderList::NewHandleIndex()
  __ZN21CAudioPlaceHolderList15FindPlaceHolderEPv   # CAudioPlaceHolderList::FindPlaceHolder(void*)
  __ZN21CAudioPlaceHolderList17AddToFreeItemListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::AddToFreeItemList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList17AddToUsedItemListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::AddToUsedItemList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList17CreatePlaceHolderEP18CCatagoryContainerPvRKN5sound12TbAudioEventE   # CAudioPlaceHolderList::CreatePlaceHolder(CCatagoryContainer*, void*, sound::TbAudioEvent const&)
  __ZN21CAudioPlaceHolderList17RemovePlaceHolderEP17CAudioPlaceHolder   # CAudioPlaceHolderList::RemovePlaceHolder(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList17UpdatePlaceHolderEPvRK15TbSoundPositionS3_   # CAudioPlaceHolderList::UpdatePlaceHolder(void*, TbSoundPosition const&, TbSoundPosition const&)
  __ZN21CAudioPlaceHolderList18MoveToFreeItemListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::MoveToFreeItemList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList18MoveToUsedItemListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::MoveToUsedItemList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList19AddToHiPriorityListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::AddToHiPriorityList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList19ProcessPlaceHoldersEv   # CAudioPlaceHolderList::ProcessPlaceHolders()
  __ZN21CAudioPlaceHolderList19SetNumberOfChannelsEj   # CAudioPlaceHolderList::SetNumberOfChannels(unsigned int)
  __ZN21CAudioPlaceHolderList19StopAllPlaceHoldersEj   # CAudioPlaceHolderList::StopAllPlaceHolders(unsigned int)
  __ZN21CAudioPlaceHolderList20NewPlaceHolderMemoryERPv   # CAudioPlaceHolderList::NewPlaceHolderMemory(void*&)
  __ZN21CAudioPlaceHolderList20SetBoundingRectangleEPvRK6TbRect   # CAudioPlaceHolderList::SetBoundingRectangle(void*, TbRect const&)
  __ZN21CAudioPlaceHolderList22AddToEndOfUsedItemListEP17CAudioPlaceHolder   # CAudioPlaceHolderList::AddToEndOfUsedItemList(CAudioPlaceHolder*)
  __ZN21CAudioPlaceHolderList22InitialisePriorityListEv   # CAudioPlaceHolderList::InitialisePriorityList()
  __ZN21CAudioPlaceHolderList26UpdatePlaceHolderParameterEPvhj   # CAudioPlaceHolderList::UpdatePlaceHolderParameter(void*, unsigned char, unsigned int)
  __ZN21CAudioPlaceHolderList28GetPlaceholderAndSoundHandleEPvRP17CAudioPlaceHolder   # CAudioPlaceHolderList::GetPlaceholderAndSoundHandle(void*, CAudioPlaceHolder*&)
  __ZN21CAudioPlaceHolderList4LoadEPN3bfl10FileSystem5CFileEb   # CAudioPlaceHolderList::Load(bfl::FileSystem::CFile*, bool)
  __ZN21CAudioPlaceHolderList4SaveEPN3bfl10FileSystem5CFileE   # CAudioPlaceHolderList::Save(bfl::FileSystem::CFile*)
  __ZN21CAudioPlaceHolderList9CMemBlock12CreateObjectEPPS0_j   # CAudioPlaceHolderList::CMemBlock::CreateObject(CAudioPlaceHolderList::CMemBlock**, unsigned int)
  __ZN21CAudioPlaceHolderList9CMemBlock4DataEv   # CAudioPlaceHolderList::CMemBlock::Data()
  __ZN21CAudioPlaceHolderList9CMemBlock7SetNextEPS0_   # CAudioPlaceHolderList::CMemBlock::SetNext(CAudioPlaceHolderList::CMemBlock*)
  __ZN21CAudioPlaceHolderList9CMemBlockC1Ej   # CAudioPlaceHolderList::CMemBlock::CMemBlock(unsigned int)
  __ZN21CAudioPlaceHolderList9CMemBlockD1Ev   # CAudioPlaceHolderList::CMemBlock::~CMemBlock()
  __ZN21CAudioPlaceHolderListC1Ej   # CAudioPlaceHolderList::CAudioPlaceHolderList(unsigned int)
  __ZN21CAudioPlaceHolderListD1Ev   # CAudioPlaceHolderList::~CAudioPlaceHolderList()
  __ZN22TbSoundCriticalSectionC1EPv   # TbSoundCriticalSection::TbSoundCriticalSection(void*)
  __ZN22TbSoundCriticalSectionD1Ev   # TbSoundCriticalSection::~TbSoundCriticalSection()
  __ZN7CLowestC1Ev   # CLowest::CLowest()
  __ZN7CLowestclEjP18TbSoundPipeChannel   # CLowest::operator()(unsigned int, TbSoundPipeChannel*)
  __ZN9TbSpeaker10AutoDelete10_mpSpeakerE   # TbSpeaker::AutoDelete::_mpSpeaker
  __ZN9TbSpeaker10AutoDeleteD1Ev   # TbSpeaker::AutoDelete::~AutoDelete()
  __ZNK12CAtomicValuecviEv   # CAtomicValue::operator int() const
  __ZNK13CUniqueHandle8GetIndexEv   # CUniqueHandle::GetIndex() const
  __ZNK13CUniqueHandlecvPvEv   # CUniqueHandle::operator void*() const
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
  __ZNK21CAudioPlaceHolderList19GetNumberOfChannelsEv   # CAudioPlaceHolderList::GetNumberOfChannels() const
  __ZNK21CAudioPlaceHolderList9CMemBlock7GetNextEv   # CAudioPlaceHolderList::CMemBlock::GetNext() const
)
set(TPW_UNIT_AudioPlaceHolderList_OWN_CALLLOG ON)
