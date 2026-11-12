# The functions of src/ tests/host_sound_wavfile.cpp builds (CMakeLists.txt, tpw_host_sound_wavfile): the sample pipe, the
# callback pipes, the PCM file codec and the timer codec, with what their tables and pools reach (the pools' words other
# than TbSoundSamplePipe's are defined in src/game/advisor/AdvisorScoring.cpp with much else: the test defines its own).
# Operator new and delete are named twice, with Windows' size_t too (unsigned long long); the slicer passes over a name a
# file does not define. The program compiles copies of these files that keep only them (cmake/slicer.cmake).
set(TPW_SOUND_WAVFILE_SOURCES
  sound/LbMpeg.cpp
  sound/StreamableMpeg2.cpp
  sound/TbSoundCodecLoop.cpp
  sound/TbSoundPipe.cpp
  sound/TbSoundPipeChannel.cpp
  sound/TbSoundSampleCallback.cpp
  sound/TbSoundSampleCodec.cpp
  sound/TbSoundSampleInfo.cpp
  sound/TbSoundSamplePipe.cpp
  sound/TbSoundSampleWAV.cpp
  sound/TbSoundSampleWAVFile.cpp
  sound/TbSoundSystemInfo.cpp
  sound/TimerCodec.cpp
  sound/UserCallback.cpp
)
set(TPW_SOUND_WAVFILE_KEEP
  __ZN11CAutoDetachC1EP17TbSoundSamplePipe   # CAutoDetach::CAutoDetach(TbSoundSamplePipe*)
  __ZN11CAutoDetachD1Ev   # CAutoDetach::~CAutoDetach()
  __ZN11CAutoDetachaSEb   # CAutoDetach::operator=(bool)
  __ZN11CMemoryCopy4CopyEPvPKvj   # CMemoryCopy::Copy(void*, void const*, unsigned int)
  __ZN12TbTimerCodec12GetDataChunkEPviRib   # TbTimerCodec::GetDataChunk(void*, int, int&, bool)
  __ZN12TbTimerCodecC1Ehj   # TbTimerCodec::TbTimerCodec(unsigned char, unsigned int)
  __ZN12TbTimerCodecD1Ev   # TbTimerCodec::~TbTimerCodec()
  __ZN12TbTimerCodecdlEPvm   # TbTimerCodec::operator delete(void*, unsigned long)
  __ZN12TbTimerCodecdlEPvy   # TbTimerCodec::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN12TbTimerCodecnwEm   # TbTimerCodec::operator new(unsigned long)
  __ZN12TbTimerCodecnwEy   # TbTimerCodec::operator new(unsigned long long) (Windows' size_t)
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
  __ZN17TbSoundSamplePipe7AttatchEPS_   # TbSoundSamplePipe::Attatch(TbSoundSamplePipe*)
  __ZN17TbSoundSamplePipe7DetatchEv   # TbSoundSamplePipe::Detatch()
  __ZN17TbSoundSamplePipe7GetInfoER17TbSoundSampleInfo   # TbSoundSamplePipe::GetInfo(TbSoundSampleInfo&)
  __ZN17TbSoundSamplePipe7ReleaseEv   # TbSoundSamplePipe::Release()
  __ZN17TbSoundSamplePipe7RestartEv   # TbSoundSamplePipe::Restart()
  __ZN17TbSoundSamplePipe7SetInfoERK17TbSoundSampleInfo   # TbSoundSamplePipe::SetInfo(TbSoundSampleInfo const&)
  __ZN17TbSoundSamplePipe8FindPipeEPv   # TbSoundSamplePipe::FindPipe(void*)
  __ZN17TbSoundSamplePipe9CompletedEb   # TbSoundSamplePipe::Completed(bool)
  __ZN17TbSoundSamplePipe9SetHandleEPv   # TbSoundSamplePipe::SetHandle(void*)
  __ZN17TbSoundSamplePipeC1Ev   # TbSoundSamplePipe::TbSoundSamplePipe()
  __ZN17TbSoundSamplePipeD1Ev   # TbSoundSamplePipe::~TbSoundSamplePipe()
  __ZN17TbSoundSamplePipedlEPvm   # TbSoundSamplePipe::operator delete(void*, unsigned long)
  __ZN17TbSoundSamplePipedlEPvy   # TbSoundSamplePipe::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN17TbSoundSamplePipenwEm   # TbSoundSamplePipe::operator new(unsigned long)
  __ZN17TbSoundSamplePipenwEy   # TbSoundSamplePipe::operator new(unsigned long long) (Windows' size_t)
  __ZN17_TbSoundSampleWAV12GetDataChunkEPviRib   # _TbSoundSampleWAV::GetDataChunk(void*, int, int&, bool)
  __ZN17_TbSoundSampleWAV12SetDataStartEPv   # _TbSoundSampleWAV::SetDataStart(void*)
  __ZN17_TbSoundSampleWAV7RestartEv   # _TbSoundSampleWAV::Restart()
  __ZN17_TbSoundSampleWAVC1EP14TbSampleHeaderP12TbRefRelease   # _TbSoundSampleWAV::_TbSoundSampleWAV(TbSampleHeader*, TbRefRelease*)
  __ZN17_TbSoundSampleWAVD1Ev   # _TbSoundSampleWAV::~_TbSoundSampleWAV()
  __ZN17_TbSoundSampleWAVdlEPvm   # _TbSoundSampleWAV::operator delete(void*, unsigned long)
  __ZN17_TbSoundSampleWAVdlEPvy   # _TbSoundSampleWAV::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN17_TbSoundSampleWAVnwEm   # _TbSoundSampleWAV::operator new(unsigned long)
  __ZN17_TbSoundSampleWAVnwEy   # _TbSoundSampleWAV::operator new(unsigned long long) (Windows' size_t)
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
  __ZN19TbSoundUserCallback7GetInfoER17TbSoundSampleInfo   # TbSoundUserCallback::GetInfo(TbSoundSampleInfo&)
  __ZN19TbSoundUserCallback7SetInfoERK17TbSoundSampleInfo   # TbSoundUserCallback::SetInfo(TbSoundSampleInfo const&)
  __ZN19TbSoundUserCallbackC1EPv   # TbSoundUserCallback::TbSoundUserCallback(void*)
  __ZN19TbSoundUserCallbackC1Ev   # TbSoundUserCallback::TbSoundUserCallback()
  __ZN19TbSoundUserCallbackD1Ev   # TbSoundUserCallback::~TbSoundUserCallback()
  __ZN19TbSoundUserCallbackdlEPvm   # TbSoundUserCallback::operator delete(void*, unsigned long)
  __ZN19TbSoundUserCallbackdlEPvy   # TbSoundUserCallback::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN19TbSoundUserCallbacknwEm   # TbSoundUserCallback::operator new(unsigned long)
  __ZN19TbSoundUserCallbacknwEy   # TbSoundUserCallback::operator new(unsigned long long) (Windows' size_t)
  __ZN21_TbSoundSampleWAVFile12GetDataChunkEPviRib   # _TbSoundSampleWAVFile::GetDataChunk(void*, int, int&, bool)
  __ZN21_TbSoundSampleWAVFile7RestartEv   # _TbSoundSampleWAVFile::Restart()
  __ZN21_TbSoundSampleWAVFileC1EPN3bfl10FileSystem5CFileEP14TbSampleHeaderjP12TbRefRelease   # _TbSoundSampleWAVFile::_TbSoundSampleWAVFile(bfl::FileSystem::CFile*, TbSampleHeader*, unsigned int, TbRefRelease*)
  __ZN21_TbSoundSampleWAVFileD1Ev   # _TbSoundSampleWAVFile::~_TbSoundSampleWAVFile()
  __ZN21_TbSoundSampleWAVFiledlEPvm   # _TbSoundSampleWAVFile::operator delete(void*, unsigned long)
  __ZN21_TbSoundSampleWAVFiledlEPvy   # _TbSoundSampleWAVFile::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN21_TbSoundSampleWAVFilenwEm   # _TbSoundSampleWAVFile::operator new(unsigned long)
  __ZN21_TbSoundSampleWAVFilenwEy   # _TbSoundSampleWAVFile::operator new(unsigned long long) (Windows' size_t)
  __ZN22_TbSoundSampleCallback12GetDataChunkEPviRib   # _TbSoundSampleCallback::GetDataChunk(void*, int, int&, bool)
  __ZN22_TbSoundSampleCallback13GetNextSampleEj   # _TbSoundSampleCallback::GetNextSample(unsigned int)
  __ZN22_TbSoundSampleCallback7GetInfoER17TbSoundSampleInfo   # _TbSoundSampleCallback::GetInfo(TbSoundSampleInfo&)
  __ZN22_TbSoundSampleCallback7SetInfoERK17TbSoundSampleInfo   # _TbSoundSampleCallback::SetInfo(TbSoundSampleInfo const&)
  __ZN22_TbSoundSampleCallbackC1EP13TbSndCallback   # _TbSoundSampleCallback::_TbSoundSampleCallback(TbSndCallback*)
  __ZN22_TbSoundSampleCallbackD1Ev   # _TbSoundSampleCallback::~_TbSoundSampleCallback()
  __ZN22_TbSoundSampleCallbackdlEPvm   # _TbSoundSampleCallback::operator delete(void*, unsigned long)
  __ZN22_TbSoundSampleCallbackdlEPvy   # _TbSoundSampleCallback::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN22_TbSoundSampleCallbacknwEm   # _TbSoundSampleCallback::operator new(unsigned long)
  __ZN22_TbSoundSampleCallbacknwEy   # _TbSoundSampleCallback::operator new(unsigned long long) (Windows' size_t)
  __ZNK18TbSoundSampleCodec15GetSampleFormatEv   # TbSoundSampleCodec::GetSampleFormat() const
  __ZN17TbSoundSampleInfo11SetCallbackEP13TbSndCallback   # TbSoundSampleInfo::SetCallback(TbSndCallback*)
  __ZN17TbSoundSampleInfo11SetUserDataEPv   # TbSoundSampleInfo::SetUserData(void*)
)
