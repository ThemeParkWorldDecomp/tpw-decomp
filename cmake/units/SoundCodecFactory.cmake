# Unit w80b: the sound library's codec factory (CCodecFactory), the bank file wrapper (_CFileAccess) and the MPEG sample codecs
# (_TbSoundSampleMPEG2, _TbSoundSampleMPEG2File). The unit file includes tests/calllog.cpp itself (it hooks the allocation
# observer). Real beside them: the WAV codecs and the loop (unit w80), the pipe base (w78), the critical sections (w79a) and the
# banks with Bank::CBank (w72, over the bullfrog strings and TbRefRelease). Fakes in the unit file: the decoder CMpegBase and the
# file player, the callback pipes, the channel's SetSampleHeader, GetNextHandle and ReleasePipe, the files, the semaphore and its
# lock, and the clock.
set(TPW_UNIT_SoundCodecFactory src/sound/FileAccess.cpp src/sound/TbSoundPipe.cpp src/sound/TbSoundSampleStore.cpp
    src/game/advisor/AdvisorScoring.cpp src/sound/StreamableMpeg2.cpp src/sound/TbBank.cpp
    src/sound/TbSoundSampleMPEG2.cpp src/sound/TbSoundSamplePipe.cpp src/sound/TbSoundCodecLoop.cpp
    src/sound/TbSoundSampleWAV.cpp src/sound/TbSoundSampleWAVFile.cpp src/sound/TimerCodec.cpp
    src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleCodec.cpp src/sound/TbSoundSystemInfo.cpp
    src/sound/Bank.cpp src/sound/ListSentence.cpp src/sound/TbMMFileBank.cpp src/sound/TbSoundBankInfo.cpp
    src/sound/TbSoundManager.cpp src/sound/TbSoundPipeChannel.cpp src/sound/TbSoundSampleInfo.cpp
    src/sound/AudioComponent.cpp src/sound/ChannelState.cpp src/sound/SystemCommand.cpp src/sound/TbSoundCore.cpp
    src/bullfrog/TbRefRelease.cpp src/bullfrog/FileSystem.cpp platform/msl.cpp)
set(TPW_UNIT_SoundCodecFactory_KEEP
  __Z8CvtDblUld   # CvtDblUl(double)
  __ZN10TbFileBank10DeregisterEv   # TbFileBank::Deregister()
  __ZN10TbFileBank12RegisterBankEPKcj   # TbFileBank::RegisterBank(char const*, unsigned int)
  __ZN10TbFileBank13GetSampleNameEj   # TbFileBank::GetSampleName(unsigned int)
  __ZN10TbFileBank14RetrieveSampleEPP14TbSampleHeaderj   # TbFileBank::RetrieveSample(TbSampleHeader**, unsigned int)
  __ZN10TbFileBankC1EP12TbRefReleasePN3bfl10FileSystem5CFileE   # TbFileBank::TbFileBank(TbRefRelease*, bfl::FileSystem::CFile*)
  __ZN10TbFileBankD1Ev   # TbFileBank::~TbFileBank()
  __ZN11CMemoryCopy4CopyEPvPKvj   # CMemoryCopy::Copy(void*, void const*, unsigned int)
  __ZN12CAtomicValueC1Ev   # CAtomicValue::CAtomicValue()
  __ZN12CAtomicValuemmEv   # CAtomicValue::operator--()
  __ZN12CAtomicValueppEv   # CAtomicValue::operator++()
  __ZN12CBankFactory12GenerateBankERK15TbSoundBankInfoP15CAudioComponent   # CBankFactory::GenerateBank(TbSoundBankInfo const&, CAudioComponent*)
  __ZN12CBankFactory13ConstructBankERP6TbBankRK15TbSoundBankInfoP15CAudioComponent   # CBankFactory::ConstructBank(TbBank*&, TbSoundBankInfo const&, CAudioComponent*)
  __ZN12CBankFactoryC1Ev   # CBankFactory::CBankFactory()
  __ZN12TbMMFileBank10DeregisterEv   # TbMMFileBank::Deregister()
  __ZN12TbMMFileBank12RegisterBankEPKcj   # TbMMFileBank::RegisterBank(char const*, unsigned int)
  __ZN12TbMMFileBank14RetrieveSampleEPP14TbSampleHeaderj   # TbMMFileBank::RetrieveSample(TbSampleHeader**, unsigned int)
  __ZN12TbMMFileBank17GetSamplePositionEj   # TbMMFileBank::GetSamplePosition(unsigned int)
  __ZN12TbMMFileBank9GetHandleEv   # TbMMFileBank::GetHandle()
  __ZN12TbMMFileBankC1EP12TbRefReleasePN3bfl10FileSystem5CFileE   # TbMMFileBank::TbMMFileBank(TbRefRelease*, bfl::FileSystem::CFile*)
  __ZN12TbMMFileBankD1Ev   # TbMMFileBank::~TbMMFileBank()
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
  __ZN12_CFileAccess15SeekToDataStartEv   # _CFileAccess::SeekToDataStart()
  __ZN12_CFileAccess4ReadEPvj   # _CFileAccess::Read(void*, unsigned int)
  __ZN12_CFileAccess4SeekEi   # _CFileAccess::Seek(int)
  __ZN12_CFileAccessC1EPN3bfl10FileSystem5CFileE   # _CFileAccess::_CFileAccess(bfl::FileSystem::CFile*)
  __ZN12_CFileAccessC1EPN3bfl10FileSystem5CFileEj   # _CFileAccess::_CFileAccess(bfl::FileSystem::CFile*, unsigned int)
  __ZN12_CFileAccessD1Ev   # _CFileAccess::~_CFileAccess()
  __ZN13CChannelState10AddChannelEv   # CChannelState::AddChannel()
  __ZN13CChannelState13RemoveChannelEv   # CChannelState::RemoveChannel()
  __ZN13CChannelState13mChannelStateE   # CChannelState::mChannelState
  __ZN13CChannelState24GetNumberOfChannelsInUseEv   # CChannelState::GetNumberOfChannelsInUse()
  __ZN13CChannelState8InstanceEv   # CChannelState::Instance()
  __ZN13CChannelStateC1Ev   # CChannelState::CChannelState()
  __ZN13CCodecFactory11CreateCodecEP18TbSoundPipeChannelR17TbSoundSampleInfo   # CCodecFactory::CreateCodec(TbSoundPipeChannel*, TbSoundSampleInfo&)
  __ZN13CCodecFactory16CreateControllerER17TbSoundSampleInfo   # CCodecFactory::CreateController(TbSoundSampleInfo&)
  __ZN13CCodecFactory19CreateCodecModifierER17TbSoundSampleInfoP14TbSampleHeaderP17TbSoundSamplePipe   # CCodecFactory::CreateCodecModifier(TbSoundSampleInfo&, TbSampleHeader*, TbSoundSamplePipe*)
  __ZN13CCodecFactory21GetNextSampleFromBankEPP14TbSampleHeaderR17TbSoundSampleInfo   # CCodecFactory::GetNextSampleFromBank(TbSampleHeader**, TbSoundSampleInfo&)
  __ZN13CCodecFactoryC1Ev   # CCodecFactory::CCodecFactory()
  __ZN13CCodecFactoryD1Ev   # CCodecFactory::~CCodecFactory()
  __ZN13CUniqueHandle14_mBitsPerCountE   # CUniqueHandle::_mBitsPerCount
  __ZN13CUniqueHandle8SetIndexEt   # CUniqueHandle::SetIndex(unsigned short)
  __ZN13CUniqueHandleC1EPv   # CUniqueHandle::CUniqueHandle(void*)
  __ZN13CUniqueHandleC1Ev   # CUniqueHandle::CUniqueHandle()
  __ZN13CUniqueHandleppEv   # CUniqueHandle::operator++()
  __ZN13SamsUtilities14UMemoryWatcherC1ERKNS_7UStringE   # SamsUtilities::UMemoryWatcher::UMemoryWatcher(SamsUtilities::UString const&)
  __ZN14CFileOperation11GetBankPathERN3bfl10FileSystem12CFileStorageER23TbDynamicStringTemplateIcERKS5_i   # CFileOperation::GetBankPath(bfl::FileSystem::CFileStorage&, TbDynamicStringTemplate<char>&, TbDynamicStringTemplate<char> const&, int)
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
  __ZN15TbSoundBankInfoC1EjPKc   # TbSoundBankInfo::TbSoundBankInfo(unsigned int, char const*)
  __ZN15TbSoundBankInfoC1EjPKcPN3bfl10FileSystem5CFileE   # TbSoundBankInfo::TbSoundBankInfo(unsigned int, char const*, bfl::FileSystem::CFile*)
  __ZN15TbSoundBankInfoC1EjPKcj   # TbSoundBankInfo::TbSoundBankInfo(unsigned int, char const*, unsigned int)
  __ZN15TbSoundBankInfoC1Ev   # TbSoundBankInfo::TbSoundBankInfo()
  __ZN17TbSoundSampleInfo7SetBankEPN4Bank5CBankE   # TbSoundSampleInfo::SetBank(Bank::CBank*)
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
  __ZN19_TbSoundSampleMPEG210ClearCacheEv   # _TbSoundSampleMPEG2::ClearCache()
  __ZN19_TbSoundSampleMPEG210FinishOpenEv   # _TbSoundSampleMPEG2::FinishOpen()
  __ZN19_TbSoundSampleMPEG212GetDataChunkEPviRib   # _TbSoundSampleMPEG2::GetDataChunk(void*, int, int&, bool)
  __ZN19_TbSoundSampleMPEG212GetMpegBlockEPvi   # _TbSoundSampleMPEG2::GetMpegBlock(void*, int)
  __ZN19_TbSoundSampleMPEG212OpenMpegFileEv   # _TbSoundSampleMPEG2::OpenMpegFile()
  __ZN19_TbSoundSampleMPEG212SetDataStartEPv   # _TbSoundSampleMPEG2::SetDataStart(void*)
  __ZN19_TbSoundSampleMPEG213CloseMpegFileEv   # _TbSoundSampleMPEG2::CloseMpegFile()
  __ZN19_TbSoundSampleMPEG215SetDataPositionEj   # _TbSoundSampleMPEG2::SetDataPosition(unsigned int)
  __ZN19_TbSoundSampleMPEG215sRestartScratchE   # _TbSoundSampleMPEG2::sRestartScratch
  __ZN19_TbSoundSampleMPEG215sSetInfoScratchE   # _TbSoundSampleMPEG2::sSetInfoScratch
  __ZN19_TbSoundSampleMPEG218SetNewDataPositionEv   # _TbSoundSampleMPEG2::SetNewDataPosition()
  __ZN19_TbSoundSampleMPEG223mLow_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mLow_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG224mHigh_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mHigh_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG225mFirst_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mFirst_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG225mLevel_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mLevel_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG225mTotal_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mTotal_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG227mCurrent_TbSoundSampleMPEG2E   # _TbSoundSampleMPEG2::mCurrent_TbSoundSampleMPEG2
  __ZN19_TbSoundSampleMPEG25TopUpEv   # _TbSoundSampleMPEG2::TopUp()
  __ZN19_TbSoundSampleMPEG27ReleaseEv   # _TbSoundSampleMPEG2::Release()
  __ZN19_TbSoundSampleMPEG27RestartEv   # _TbSoundSampleMPEG2::Restart()
  __ZN19_TbSoundSampleMPEG27SetInfoERK17TbSoundSampleInfo   # _TbSoundSampleMPEG2::SetInfo(TbSoundSampleInfo const&)
  __ZN19_TbSoundSampleMPEG28SetLevelEi   # _TbSoundSampleMPEG2::SetLevel(int)
  __ZN19_TbSoundSampleMPEG2C1EP14TbSampleHeaderP12TbRefRelease   # _TbSoundSampleMPEG2::_TbSoundSampleMPEG2(TbSampleHeader*, TbRefRelease*)
  __ZN19_TbSoundSampleMPEG2D1Ev   # _TbSoundSampleMPEG2::~_TbSoundSampleMPEG2()
  __ZN19_TbSoundSampleMPEG2dlEPvm   # _TbSoundSampleMPEG2::operator delete(void*, unsigned long)
  __ZN19_TbSoundSampleMPEG2nwEm   # _TbSoundSampleMPEG2::operator new(unsigned long)
  __ZN20CAutoCriticalSectionC1Ev   # CAutoCriticalSection::CAutoCriticalSection()
  __ZN20CAutoCriticalSectionD1Ev   # CAutoCriticalSection::~CAutoCriticalSection()
  __ZN20CAutoCriticalSectioncvPPvEv   # CAutoCriticalSection::operator void**()
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
  __ZN22TbSoundCriticalSectionC1EPv   # TbSoundCriticalSection::TbSoundCriticalSection(void*)
  __ZN22TbSoundCriticalSectionD1Ev   # TbSoundCriticalSection::~TbSoundCriticalSection()
  __ZN23_CS__TbSoundSampleMPEG2D1Ev   # _CS__TbSoundSampleMPEG2::~_CS__TbSoundSampleMPEG2()
  __ZN23_TbSoundSampleMPEG2File10ClearCacheEv   # _TbSoundSampleMPEG2File::ClearCache()
  __ZN23_TbSoundSampleMPEG2File12GetMpegBlockEPvi   # _TbSoundSampleMPEG2File::GetMpegBlock(void*, int)
  __ZN23_TbSoundSampleMPEG2File12OpenMpegFileEv   # _TbSoundSampleMPEG2File::OpenMpegFile()
  __ZN23_TbSoundSampleMPEG2File13CloseMpegFileEv   # _TbSoundSampleMPEG2File::CloseMpegFile()
  __ZN23_TbSoundSampleMPEG2File18SetNewDataPositionEv   # _TbSoundSampleMPEG2File::SetNewDataPosition()
  __ZN23_TbSoundSampleMPEG2File27mLow_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mLow_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File28mHigh_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mHigh_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File29mFirst_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mFirst_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File29mLevel_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mLevel_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File29mTotal_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mTotal_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File31mCurrent_TbSoundSampleMPEG2FileE   # _TbSoundSampleMPEG2File::mCurrent_TbSoundSampleMPEG2File
  __ZN23_TbSoundSampleMPEG2File4ReadEPvj   # _TbSoundSampleMPEG2File::Read(void*, unsigned int)
  __ZN23_TbSoundSampleMPEG2File4SeekEi   # _TbSoundSampleMPEG2File::Seek(int)
  __ZN23_TbSoundSampleMPEG2File5TopUpEv   # _TbSoundSampleMPEG2File::TopUp()
  __ZN23_TbSoundSampleMPEG2File7RestartEv   # _TbSoundSampleMPEG2File::Restart()
  __ZN23_TbSoundSampleMPEG2File8SetLevelEi   # _TbSoundSampleMPEG2File::SetLevel(int)
  __ZN23_TbSoundSampleMPEG2FileC1EPN3bfl10FileSystem5CFileEP14TbSampleHeaderjP12TbRefRelease   # _TbSoundSampleMPEG2File::_TbSoundSampleMPEG2File(bfl::FileSystem::CFile*, TbSampleHeader*, unsigned int, TbRefRelease*)
  __ZN23_TbSoundSampleMPEG2FileD1Ev   # _TbSoundSampleMPEG2File::~_TbSoundSampleMPEG2File()
  __ZN23_TbSoundSampleMPEG2FiledlEPvm   # _TbSoundSampleMPEG2File::operator delete(void*, unsigned long)
  __ZN23_TbSoundSampleMPEG2FilenwEm   # _TbSoundSampleMPEG2File::operator new(unsigned long)
  __ZN27_CS__TbSoundSampleMPEG2FileD1Ev   # _CS__TbSoundSampleMPEG2File::~_CS__TbSoundSampleMPEG2File()
  __ZN4Bank5CBankC1EP6TbBank   # Bank::CBank::CBank(TbBank*)
  __ZN4Bank5CBankD1Ev   # Bank::CBank::~CBank()
  __ZN6TbBank10InitialiseEPP12TbRefRelease   # TbBank::Initialise(TbRefRelease**)
  __ZN6TbBank13GetSampleNameEj   # TbBank::GetSampleName(unsigned int)
  __ZN6TbBank15GetSampleLengthEj   # TbBank::GetSampleLength(unsigned int)
  __ZN6TbBank17GetSamplePositionEj   # TbBank::GetSamplePosition(unsigned int)
  __ZN6TbBank18GetNumberOfSamplesEv   # TbBank::GetNumberOfSamples()
  __ZN6TbBank9GetHandleEv   # TbBank::GetHandle()
  __ZN6TbBankC1EP12TbRefRelease   # TbBank::TbBank(TbRefRelease*)
  __ZN6TbBankD1Ev   # TbBank::~TbBank()
  __ZN7CLowestC1Ev   # CLowest::CLowest()
  __ZN7CLowestclEjP18TbSoundPipeChannel   # CLowest::operator()(unsigned int, TbSoundPipeChannel*)
  __ZN9TbSpeaker10AutoDelete10_mpSpeakerE   # TbSpeaker::AutoDelete::_mpSpeaker
  __ZN9TbSpeaker10AutoDeleteD1Ev   # TbSpeaker::AutoDelete::~AutoDelete()
  __ZNK10TbFileBank6IsTypeEj   # TbFileBank::IsType(unsigned int) const
  __ZNK12CAtomicValuecviEv   # CAtomicValue::operator int() const
  __ZNK12TbMMFileBank11IsStreamingEv   # TbMMFileBank::IsStreaming() const
  __ZNK12TbMMFileBank6IsTypeEj   # TbMMFileBank::IsType(unsigned int) const
  __ZNK13CUniqueHandle8GetIndexEv   # CUniqueHandle::GetIndex() const
  __ZNK13CUniqueHandlecvPvEv   # CUniqueHandle::operator void*() const
  __ZNK15TbSoundBankInfo7GetFileEv   # TbSoundBankInfo::GetFile() const
  __ZNK15TbSoundBankInfo7GetPathEv   # TbSoundBankInfo::GetPath() const
  __ZNK15TbSoundBankInfo7GetSizeEv   # TbSoundBankInfo::GetSize() const
  __ZNK18TbSoundSampleCodec15GetSampleFormatEv   # TbSoundSampleCodec::GetSampleFormat() const
  __ZNK4Bank5CBank11IsStreamingEv   # Bank::CBank::IsStreaming() const
  __ZNK4Bank5CBank14RetrieveSampleEPP14TbSampleHeaderj   # Bank::CBank::RetrieveSample(TbSampleHeader**, unsigned int) const
  __ZNK4Bank5CBank17GetSamplePositionEj   # Bank::CBank::GetSamplePosition(unsigned int) const
  __ZNK4Bank5CBank19GetBankAsRefReleaseEv   # Bank::CBank::GetBankAsRefRelease() const
  __ZNK4Bank5CBankcvPN3bfl10FileSystem5CFileEEv   # Bank::CBank::operator bfl::FileSystem::CFile*() const
  __ZNK6TbBank11IsStreamingEv   # TbBank::IsStreaming() const
  __ZN12_GLOBAL__N_113sCleanupMPEG2E   # sCleanupMPEG2 (static object)
  __ZN12_GLOBAL__N_117sCleanupMPEG2FileE   # sCleanupMPEG2File (static object)
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
set(TPW_UNIT_SoundCodecFactory_OWN_CALLLOG ON)
