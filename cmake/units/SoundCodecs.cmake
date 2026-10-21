# Unit w80: the sound library's four small codecs (_TbSoundSampleWAV, _TbSoundSampleWAVFile, _TbSoundCodecLoop, TbTimerCodec) and
# CMemoryCopy::Copy. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer); the codecs derive from
# unit w78's classes (src/sound/StreamableMpeg2.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleCodec.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSystemInfo.cpp) and TbRefRelease (real) is the base of the fake owner. The clock (LbTime_GetClock),
# the file (bfl::FileSystem::CFile) and the codec behind a loop are fakes in the unit file.
set(TPW_UNIT_SoundCodecs src/game/advisor/AdvisorScoring.cpp src/sound/TbSoundCodecLoop.cpp src/sound/TbSoundPipe.cpp
    src/sound/TbSoundSampleWAV.cpp src/sound/TbSoundSampleWAVFile.cpp src/sound/TimerCodec.cpp
    src/sound/StreamableMpeg2.cpp src/sound/TbSoundSampleCallback.cpp src/sound/TbSoundSampleCodec.cpp
    src/sound/TbSoundSamplePipe.cpp src/sound/TbSoundSystemInfo.cpp src/bullfrog/TbRefRelease.cpp)
set(TPW_UNIT_SoundCodecs_KEEP
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
  __ZNK18TbSoundSampleCodec15GetSampleFormatEv   # TbSoundSampleCodec::GetSampleFormat() const
)
set(TPW_UNIT_SoundCodecs_OWN_CALLLOG ON)
