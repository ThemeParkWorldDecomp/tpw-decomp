# Unit w79i: the sound library's sample callback pipes (TbSoundUserCallback, _TbSoundSampleCallback,
# _TbSoundSampleCompletedCallback, CAutoDetach, their pools and cleanup objects). The unit file includes tests/calllog.cpp itself
# (it hooks the allocation observer); real beside them are the pipe base (w78, src/sound/StreamableMpeg2.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleCodec.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSystemInfo.cpp), the sample info setters
# (w77, src/sound/AudioPlaceHolder.cpp, src/sound/SystemCommand.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundManager.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundPosition.cpp, src/sound/TbSoundSampleInfo.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TbSoundSystemInfo.cpp, src/sound/TbSysCommand.cpp), the timer codec (w80, src/game/advisor/AdvisorScoring.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleWAV.cpp, src/sound/TbSoundSampleWAVFile.cpp, src/sound/TimerCodec.cpp) and TbRefRelease. Fakes in the unit file: the next
# codec behind a pipe, the TbSndCallback, the open file class and the clock.
set(TPW_UNIT_SoundCallbackPipes src/game/advisor/AdvisorScoring.cpp src/sound/TbSoundBankInfo.cpp
    src/sound/TbSoundPipe.cpp src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleCodec.cpp
    src/sound/TbSoundSamplePipe.cpp src/sound/TbSoundSampleStore.cpp src/sound/UserCallback.cpp
    src/sound/StreamableMpeg2.cpp src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundSystemInfo.cpp
    src/sound/AudioPlaceHolder.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCore.cpp src/sound/TbSoundManager.cpp
    src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundPosition.cpp src/sound/TbSoundSampleInfo.cpp
    src/sound/TbSysCommand.cpp src/sound/TbSoundSampleWAV.cpp src/sound/TbSoundSampleWAVFile.cpp
    src/sound/TimerCodec.cpp src/bullfrog/TbRefRelease.cpp)
set(TPW_UNIT_SoundCallbackPipes_KEEP
  __ZN11CAutoDetachC1EP17TbSoundSamplePipe   # CAutoDetach::CAutoDetach(TbSoundSamplePipe*)
  __ZN11CAutoDetachD1Ev   # CAutoDetach::~CAutoDetach()
  __ZN11CAutoDetachaSEb   # CAutoDetach::operator=(bool)
  __ZN11CMemoryCopy4CopyEPvPKvj   # CMemoryCopy::Copy(void*, void const*, unsigned int)
  __ZN12TbRefRelease6AddRefEv   # TbRefRelease::AddRef()
  __ZN12TbRefRelease7ReleaseEv   # TbRefRelease::Release()
  __ZN12TbRefReleaseC1Ev   # TbRefRelease::TbRefRelease()
  __ZN12TbRefReleaseD1Ev   # TbRefRelease::~TbRefRelease()
  __ZN12TbTimerCodec10ClearCacheEv   # TbTimerCodec::ClearCache()
  __ZN12TbTimerCodec12GetDataChunkEPviRib   # TbTimerCodec::GetDataChunk(void*, int, int&, bool)
  __ZN12TbTimerCodec16mLowTbTimerCodecE   # TbTimerCodec::mLowTbTimerCodec
  __ZN12TbTimerCodec17mHighTbTimerCodecE   # TbTimerCodec::mHighTbTimerCodec
  __ZN12TbTimerCodec18mFirstTbTimerCodecE   # TbTimerCodec::mFirstTbTimerCodec
  __ZN12TbTimerCodec18mLevelTbTimerCodecE   # TbTimerCodec::mLevelTbTimerCodec
  __ZN12TbTimerCodec18mTotalTbTimerCodecE   # TbTimerCodec::mTotalTbTimerCodec
  __ZN12TbTimerCodec20mCurrentTbTimerCodecE   # TbTimerCodec::mCurrentTbTimerCodec
  __ZN12TbTimerCodec5TopUpEv   # TbTimerCodec::TopUp()
  __ZN12TbTimerCodec8SetLevelEi   # TbTimerCodec::SetLevel(int)
  __ZN12TbTimerCodecC1Ehj   # TbTimerCodec::TbTimerCodec(unsigned char, unsigned int)
  __ZN12TbTimerCodecD1Ev   # TbTimerCodec::~TbTimerCodec()
  __ZN12TbTimerCodecdlEPvm   # TbTimerCodec::operator delete(void*, unsigned long)
  __ZN12TbTimerCodecnwEm   # TbTimerCodec::operator new(unsigned long)
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
  __ZN17_TbSoundCodecLoop10ClearCacheEv   # _TbSoundCodecLoop::ClearCache()
  __ZN17_TbSoundCodecLoop12GetDataChunkEPviRib   # _TbSoundCodecLoop::GetDataChunk(void*, int, int&, bool)
  __ZN17_TbSoundCodecLoop21mLow_TbSoundCodecLoopE   # _TbSoundCodecLoop::mLow_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop22mHigh_TbSoundCodecLoopE   # _TbSoundCodecLoop::mHigh_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop23mFirst_TbSoundCodecLoopE   # _TbSoundCodecLoop::mFirst_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop23mLevel_TbSoundCodecLoopE   # _TbSoundCodecLoop::mLevel_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop23mTotal_TbSoundCodecLoopE   # _TbSoundCodecLoop::mTotal_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop25mCurrent_TbSoundCodecLoopE   # _TbSoundCodecLoop::mCurrent_TbSoundCodecLoop
  __ZN17_TbSoundCodecLoop5TopUpEv   # _TbSoundCodecLoop::TopUp()
  __ZN17_TbSoundCodecLoop8SetLevelEi   # _TbSoundCodecLoop::SetLevel(int)
  __ZN17_TbSoundCodecLoopC1Ev   # _TbSoundCodecLoop::_TbSoundCodecLoop()
  __ZN17_TbSoundCodecLoopD1Ev   # _TbSoundCodecLoop::~_TbSoundCodecLoop()
  __ZN17_TbSoundCodecLoopdlEPvm   # _TbSoundCodecLoop::operator delete(void*, unsigned long)
  __ZN17_TbSoundCodecLoopnwEm   # _TbSoundCodecLoop::operator new(unsigned long)
  __ZN17_TbSoundSampleWAV10ClearCacheEv   # _TbSoundSampleWAV::ClearCache()
  __ZN17_TbSoundSampleWAV12GetDataChunkEPviRib   # _TbSoundSampleWAV::GetDataChunk(void*, int, int&, bool)
  __ZN17_TbSoundSampleWAV12SetDataStartEPv   # _TbSoundSampleWAV::SetDataStart(void*)
  __ZN17_TbSoundSampleWAV21mLow_TbSoundSampleWAVE   # _TbSoundSampleWAV::mLow_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV22mHigh_TbSoundSampleWAVE   # _TbSoundSampleWAV::mHigh_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV23mFirst_TbSoundSampleWAVE   # _TbSoundSampleWAV::mFirst_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV23mLevel_TbSoundSampleWAVE   # _TbSoundSampleWAV::mLevel_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV23mTotal_TbSoundSampleWAVE   # _TbSoundSampleWAV::mTotal_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV25mCurrent_TbSoundSampleWAVE   # _TbSoundSampleWAV::mCurrent_TbSoundSampleWAV
  __ZN17_TbSoundSampleWAV5TopUpEv   # _TbSoundSampleWAV::TopUp()
  __ZN17_TbSoundSampleWAV7RestartEv   # _TbSoundSampleWAV::Restart()
  __ZN17_TbSoundSampleWAV8SetLevelEi   # _TbSoundSampleWAV::SetLevel(int)
  __ZN17_TbSoundSampleWAVC1EP14TbSampleHeaderP12TbRefRelease   # _TbSoundSampleWAV::_TbSoundSampleWAV(TbSampleHeader*, TbRefRelease*)
  __ZN17_TbSoundSampleWAVD1Ev   # _TbSoundSampleWAV::~_TbSoundSampleWAV()
  __ZN17_TbSoundSampleWAVdlEPvm   # _TbSoundSampleWAV::operator delete(void*, unsigned long)
  __ZN17_TbSoundSampleWAVnwEm   # _TbSoundSampleWAV::operator new(unsigned long)
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
  __ZN19TbSoundUserCallback10ClearCacheEv   # TbSoundUserCallback::ClearCache()
  __ZN19TbSoundUserCallback23mLowTbSoundUserCallbackE   # TbSoundUserCallback::mLowTbSoundUserCallback
  __ZN19TbSoundUserCallback24mHighTbSoundUserCallbackE   # TbSoundUserCallback::mHighTbSoundUserCallback
  __ZN19TbSoundUserCallback25mFirstTbSoundUserCallbackE   # TbSoundUserCallback::mFirstTbSoundUserCallback
  __ZN19TbSoundUserCallback25mLevelTbSoundUserCallbackE   # TbSoundUserCallback::mLevelTbSoundUserCallback
  __ZN19TbSoundUserCallback25mTotalTbSoundUserCallbackE   # TbSoundUserCallback::mTotalTbSoundUserCallback
  __ZN19TbSoundUserCallback27mCurrentTbSoundUserCallbackE   # TbSoundUserCallback::mCurrentTbSoundUserCallback
  __ZN19TbSoundUserCallback5TopUpEv   # TbSoundUserCallback::TopUp()
  __ZN19TbSoundUserCallback7GetInfoER17TbSoundSampleInfo   # TbSoundUserCallback::GetInfo(TbSoundSampleInfo&)
  __ZN19TbSoundUserCallback7SetInfoERK17TbSoundSampleInfo   # TbSoundUserCallback::SetInfo(TbSoundSampleInfo const&)
  __ZN19TbSoundUserCallback8SetLevelEi   # TbSoundUserCallback::SetLevel(int)
  __ZN19TbSoundUserCallbackC1EPv   # TbSoundUserCallback::TbSoundUserCallback(void*)
  __ZN19TbSoundUserCallbackC1Ev   # TbSoundUserCallback::TbSoundUserCallback()
  __ZN19TbSoundUserCallbackD1Ev   # TbSoundUserCallback::~TbSoundUserCallback()
  __ZN19TbSoundUserCallbackdlEPvm   # TbSoundUserCallback::operator delete(void*, unsigned long)
  __ZN19TbSoundUserCallbacknwEm   # TbSoundUserCallback::operator new(unsigned long)
  __ZN20TbSoundVolumeControl10SetVolumesEPf   # TbSoundVolumeControl::SetVolumes(float*)
  __ZN20TbSoundVolumeControl4InitEj   # TbSoundVolumeControl::Init(unsigned int)
  __ZN20TbSoundVolumeControl9GetVolumeEj   # TbSoundVolumeControl::GetVolume(unsigned int)
  __ZN20TbSoundVolumeControlC1Ev   # TbSoundVolumeControl::TbSoundVolumeControl()
  __ZN21_TbSoundSampleWAVFile10ClearCacheEv   # _TbSoundSampleWAVFile::ClearCache()
  __ZN21_TbSoundSampleWAVFile12GetDataChunkEPviRib   # _TbSoundSampleWAVFile::GetDataChunk(void*, int, int&, bool)
  __ZN21_TbSoundSampleWAVFile25mLow_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mLow_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile26mHigh_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mHigh_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile27mFirst_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mFirst_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile27mLevel_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mLevel_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile27mTotal_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mTotal_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile29mCurrent_TbSoundSampleWAVFileE   # _TbSoundSampleWAVFile::mCurrent_TbSoundSampleWAVFile
  __ZN21_TbSoundSampleWAVFile5TopUpEv   # _TbSoundSampleWAVFile::TopUp()
  __ZN21_TbSoundSampleWAVFile7RestartEv   # _TbSoundSampleWAVFile::Restart()
  __ZN21_TbSoundSampleWAVFile8SetLevelEi   # _TbSoundSampleWAVFile::SetLevel(int)
  __ZN21_TbSoundSampleWAVFileC1EPN3bfl10FileSystem5CFileEP14TbSampleHeaderjP12TbRefRelease   # _TbSoundSampleWAVFile::_TbSoundSampleWAVFile(bfl::FileSystem::CFile*, TbSampleHeader*, unsigned int, TbRefRelease*)
  __ZN21_TbSoundSampleWAVFileD1Ev   # _TbSoundSampleWAVFile::~_TbSoundSampleWAVFile()
  __ZN21_TbSoundSampleWAVFiledlEPvm   # _TbSoundSampleWAVFile::operator delete(void*, unsigned long)
  __ZN21_TbSoundSampleWAVFilenwEm   # _TbSoundSampleWAVFile::operator new(unsigned long)
  __ZN22_TbSoundSampleCallback10ClearCacheEv   # _TbSoundSampleCallback::ClearCache()
  __ZN22_TbSoundSampleCallback12GetDataChunkEPviRib   # _TbSoundSampleCallback::GetDataChunk(void*, int, int&, bool)
  __ZN22_TbSoundSampleCallback13GetNextSampleEj   # _TbSoundSampleCallback::GetNextSample(unsigned int)
  __ZN22_TbSoundSampleCallback26mLow_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mLow_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback27mHigh_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mHigh_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback28mFirst_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mFirst_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback28mLevel_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mLevel_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback28mTotal_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mTotal_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback30mCurrent_TbSoundSampleCallbackE   # _TbSoundSampleCallback::mCurrent_TbSoundSampleCallback
  __ZN22_TbSoundSampleCallback5TopUpEv   # _TbSoundSampleCallback::TopUp()
  __ZN22_TbSoundSampleCallback7GetInfoER17TbSoundSampleInfo   # _TbSoundSampleCallback::GetInfo(TbSoundSampleInfo&)
  __ZN22_TbSoundSampleCallback7SetInfoERK17TbSoundSampleInfo   # _TbSoundSampleCallback::SetInfo(TbSoundSampleInfo const&)
  __ZN22_TbSoundSampleCallback8SetLevelEi   # _TbSoundSampleCallback::SetLevel(int)
  __ZN22_TbSoundSampleCallbackC1EP13TbSndCallback   # _TbSoundSampleCallback::_TbSoundSampleCallback(TbSndCallback*)
  __ZN22_TbSoundSampleCallbackD1Ev   # _TbSoundSampleCallback::~_TbSoundSampleCallback()
  __ZN22_TbSoundSampleCallbackdlEPvm   # _TbSoundSampleCallback::operator delete(void*, unsigned long)
  __ZN22_TbSoundSampleCallbacknwEm   # _TbSoundSampleCallback::operator new(unsigned long)
  __ZN23_CS_TbSoundUserCallbackD1Ev   # _CS_TbSoundUserCallback::~_CS_TbSoundUserCallback()
  __ZN26_CS__TbSoundSampleCallbackD1Ev   # _CS__TbSoundSampleCallback::~_CS__TbSoundSampleCallback()
  __ZN31_TbSoundSampleCompletedCallback10ClearCacheEv   # _TbSoundSampleCompletedCallback::ClearCache()
  __ZN31_TbSoundSampleCompletedCallback12GetDataChunkEPviRib   # _TbSoundSampleCompletedCallback::GetDataChunk(void*, int, int&, bool)
  __ZN31_TbSoundSampleCompletedCallback35mLow_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mLow_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback36mHigh_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mHigh_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback37mFirst_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mFirst_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback37mLevel_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mLevel_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback37mTotal_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mTotal_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback39mCurrent_TbSoundSampleCompletedCallbackE   # _TbSoundSampleCompletedCallback::mCurrent_TbSoundSampleCompletedCallback
  __ZN31_TbSoundSampleCompletedCallback5TopUpEv   # _TbSoundSampleCompletedCallback::TopUp()
  __ZN31_TbSoundSampleCompletedCallback8SetLevelEi   # _TbSoundSampleCompletedCallback::SetLevel(int)
  __ZN31_TbSoundSampleCompletedCallbackC1EP13TbSndCallbackPv   # _TbSoundSampleCompletedCallback::_TbSoundSampleCompletedCallback(TbSndCallback*, void*)
  __ZN31_TbSoundSampleCompletedCallbackD1Ev   # _TbSoundSampleCompletedCallback::~_TbSoundSampleCompletedCallback()
  __ZN31_TbSoundSampleCompletedCallbackdlEPvm   # _TbSoundSampleCompletedCallback::operator delete(void*, unsigned long)
  __ZN31_TbSoundSampleCompletedCallbacknwEm   # _TbSoundSampleCompletedCallback::operator new(unsigned long)
  __ZN35_CS__TbSoundSampleCompletedCallbackD1Ev   # _CS__TbSoundSampleCompletedCallback::~_CS__TbSoundSampleCompletedCallback()
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
  __ZN12_GLOBAL__N_120sCleanupUserCallbackE   # sCleanupUserCallback (static object)
  __ZN12_GLOBAL__N_122sCleanupSampleCallbackE   # sCleanupSampleCallback (static object)
  __ZN12_GLOBAL__N_125sCleanupCompletedCallbackE   # sCleanupCompletedCallback (static object)
)
set(TPW_UNIT_SoundCallbackPipes_OWN_CALLLOG ON)
