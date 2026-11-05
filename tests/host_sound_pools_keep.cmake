# The functions of src/ tests/host_sound_pools.cpp builds (CMakeLists.txt, tpw_host_sound_pools): the take (operator new),
# give-back (operator delete), TopUp and ClearCache of each pool of the sound library, and TbSoundSamplePipe's pool words
# (the other pools' words are defined in src/game/advisor/AdvisorScoring.cpp and src/sound/MpegDecode.cpp, with much else:
# the test defines its own). Operator new and delete are named twice, with Windows' size_t too (unsigned long long); the
# slicer passes over a name a file does not define. The program compiles copies of these files that keep only them (cmake/slicer.cmake).
set(TPW_SOUND_POOLS_SOURCES
  sound/LbMpeg.cpp
  sound/StreamableMpeg2.cpp
  sound/TbSoundCodecLoop.cpp
  sound/TbSoundSampleCallback.cpp
  sound/TbSoundSampleMPEG2.cpp
  sound/TbSoundSamplePipe.cpp
  sound/TbSoundSampleWAV.cpp
  sound/TbSoundSampleWAVFile.cpp
  sound/TimerCodec.cpp
  sound/UserCallback.cpp
)
set(TPW_SOUND_POOLS_KEEP
  __ZN9CMpegBase10ClearCacheEv   # CMpegBase::ClearCache()
  __ZN9CMpegBase5TopUpEv   # CMpegBase::TopUp()
  __ZN9CMpegBasedlEPvm   # CMpegBase::operator delete(void*, unsigned long)
  __ZN9CMpegBasedlEPvy   # CMpegBase::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN9CMpegBasenwEm   # CMpegBase::operator new(unsigned long)
  __ZN9CMpegBasenwEy   # CMpegBase::operator new(unsigned long long) (Windows' size_t)
  __ZN23_TbSoundSampleMPEG2File10ClearCacheEv   # _TbSoundSampleMPEG2File::ClearCache()
  __ZN23_TbSoundSampleMPEG2File5TopUpEv   # _TbSoundSampleMPEG2File::TopUp()
  __ZN23_TbSoundSampleMPEG2FiledlEPvm   # _TbSoundSampleMPEG2File::operator delete(void*, unsigned long)
  __ZN23_TbSoundSampleMPEG2FiledlEPvy   # _TbSoundSampleMPEG2File::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN23_TbSoundSampleMPEG2FilenwEm   # _TbSoundSampleMPEG2File::operator new(unsigned long)
  __ZN23_TbSoundSampleMPEG2FilenwEy   # _TbSoundSampleMPEG2File::operator new(unsigned long long) (Windows' size_t)
  __ZN17_TbSoundCodecLoop10ClearCacheEv   # _TbSoundCodecLoop::ClearCache()
  __ZN17_TbSoundCodecLoop5TopUpEv   # _TbSoundCodecLoop::TopUp()
  __ZN17_TbSoundCodecLoopdlEPvm   # _TbSoundCodecLoop::operator delete(void*, unsigned long)
  __ZN17_TbSoundCodecLoopdlEPvy   # _TbSoundCodecLoop::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN17_TbSoundCodecLoopnwEm   # _TbSoundCodecLoop::operator new(unsigned long)
  __ZN17_TbSoundCodecLoopnwEy   # _TbSoundCodecLoop::operator new(unsigned long long) (Windows' size_t)
  __ZN22_TbSoundSampleCallback10ClearCacheEv   # _TbSoundSampleCallback::ClearCache()
  __ZN22_TbSoundSampleCallback5TopUpEv   # _TbSoundSampleCallback::TopUp()
  __ZN22_TbSoundSampleCallbackdlEPvm   # _TbSoundSampleCallback::operator delete(void*, unsigned long)
  __ZN22_TbSoundSampleCallbackdlEPvy   # _TbSoundSampleCallback::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN22_TbSoundSampleCallbacknwEm   # _TbSoundSampleCallback::operator new(unsigned long)
  __ZN22_TbSoundSampleCallbacknwEy   # _TbSoundSampleCallback::operator new(unsigned long long) (Windows' size_t)
  __ZN19_TbSoundSampleMPEG210ClearCacheEv   # _TbSoundSampleMPEG2::ClearCache()
  __ZN19_TbSoundSampleMPEG25TopUpEv   # _TbSoundSampleMPEG2::TopUp()
  __ZN19_TbSoundSampleMPEG2dlEPvm   # _TbSoundSampleMPEG2::operator delete(void*, unsigned long)
  __ZN19_TbSoundSampleMPEG2dlEPvy   # _TbSoundSampleMPEG2::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN19_TbSoundSampleMPEG2nwEm   # _TbSoundSampleMPEG2::operator new(unsigned long)
  __ZN19_TbSoundSampleMPEG2nwEy   # _TbSoundSampleMPEG2::operator new(unsigned long long) (Windows' size_t)
  __ZN17TbSoundSamplePipe10ClearCacheEv   # TbSoundSamplePipe::ClearCache()
  __ZN17TbSoundSamplePipe21mLowTbSoundSamplePipeE   # TbSoundSamplePipe::mLowTbSoundSamplePipe
  __ZN17TbSoundSamplePipe22mHighTbSoundSamplePipeE   # TbSoundSamplePipe::mHighTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mFirstTbSoundSamplePipeE   # TbSoundSamplePipe::mFirstTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mLevelTbSoundSamplePipeE   # TbSoundSamplePipe::mLevelTbSoundSamplePipe
  __ZN17TbSoundSamplePipe23mTotalTbSoundSamplePipeE   # TbSoundSamplePipe::mTotalTbSoundSamplePipe
  __ZN17TbSoundSamplePipe25mCurrentTbSoundSamplePipeE   # TbSoundSamplePipe::mCurrentTbSoundSamplePipe
  __ZN17TbSoundSamplePipe5TopUpEv   # TbSoundSamplePipe::TopUp()
  __ZN17TbSoundSamplePipedlEPvm   # TbSoundSamplePipe::operator delete(void*, unsigned long)
  __ZN17TbSoundSamplePipedlEPvy   # TbSoundSamplePipe::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN17TbSoundSamplePipenwEm   # TbSoundSamplePipe::operator new(unsigned long)
  __ZN17TbSoundSamplePipenwEy   # TbSoundSamplePipe::operator new(unsigned long long) (Windows' size_t)
  __ZN31_TbSoundSampleCompletedCallback10ClearCacheEv   # _TbSoundSampleCompletedCallback::ClearCache()
  __ZN31_TbSoundSampleCompletedCallback5TopUpEv   # _TbSoundSampleCompletedCallback::TopUp()
  __ZN31_TbSoundSampleCompletedCallbackdlEPvm   # _TbSoundSampleCompletedCallback::operator delete(void*, unsigned long)
  __ZN31_TbSoundSampleCompletedCallbackdlEPvy   # _TbSoundSampleCompletedCallback::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN31_TbSoundSampleCompletedCallbacknwEm   # _TbSoundSampleCompletedCallback::operator new(unsigned long)
  __ZN31_TbSoundSampleCompletedCallbacknwEy   # _TbSoundSampleCompletedCallback::operator new(unsigned long long) (Windows' size_t)
  __ZN17_TbSoundSampleWAV10ClearCacheEv   # _TbSoundSampleWAV::ClearCache()
  __ZN17_TbSoundSampleWAV5TopUpEv   # _TbSoundSampleWAV::TopUp()
  __ZN17_TbSoundSampleWAVdlEPvm   # _TbSoundSampleWAV::operator delete(void*, unsigned long)
  __ZN17_TbSoundSampleWAVdlEPvy   # _TbSoundSampleWAV::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN17_TbSoundSampleWAVnwEm   # _TbSoundSampleWAV::operator new(unsigned long)
  __ZN17_TbSoundSampleWAVnwEy   # _TbSoundSampleWAV::operator new(unsigned long long) (Windows' size_t)
  __ZN21_TbSoundSampleWAVFile10ClearCacheEv   # _TbSoundSampleWAVFile::ClearCache()
  __ZN21_TbSoundSampleWAVFile5TopUpEv   # _TbSoundSampleWAVFile::TopUp()
  __ZN21_TbSoundSampleWAVFiledlEPvm   # _TbSoundSampleWAVFile::operator delete(void*, unsigned long)
  __ZN21_TbSoundSampleWAVFiledlEPvy   # _TbSoundSampleWAVFile::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN21_TbSoundSampleWAVFilenwEm   # _TbSoundSampleWAVFile::operator new(unsigned long)
  __ZN21_TbSoundSampleWAVFilenwEy   # _TbSoundSampleWAVFile::operator new(unsigned long long) (Windows' size_t)
  __ZN12TbTimerCodec10ClearCacheEv   # TbTimerCodec::ClearCache()
  __ZN12TbTimerCodec5TopUpEv   # TbTimerCodec::TopUp()
  __ZN12TbTimerCodecdlEPvm   # TbTimerCodec::operator delete(void*, unsigned long)
  __ZN12TbTimerCodecdlEPvy   # TbTimerCodec::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN12TbTimerCodecnwEm   # TbTimerCodec::operator new(unsigned long)
  __ZN12TbTimerCodecnwEy   # TbTimerCodec::operator new(unsigned long long) (Windows' size_t)
  __ZN19TbSoundUserCallback10ClearCacheEv   # TbSoundUserCallback::ClearCache()
  __ZN19TbSoundUserCallback5TopUpEv   # TbSoundUserCallback::TopUp()
  __ZN19TbSoundUserCallbackdlEPvm   # TbSoundUserCallback::operator delete(void*, unsigned long)
  __ZN19TbSoundUserCallbackdlEPvy   # TbSoundUserCallback::operator delete(void*, unsigned long long) (Windows' size_t)
  __ZN19TbSoundUserCallbacknwEm   # TbSoundUserCallback::operator new(unsigned long)
  __ZN19TbSoundUserCallbacknwEy   # TbSoundUserCallback::operator new(unsigned long long) (Windows' size_t)
)
