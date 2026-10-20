# Unit w80b: the sound library's codec factory (CCodecFactory), the bank file wrapper (_CFileAccess) and the MPEG sample codecs
# (_TbSoundSampleMPEG2, _TbSoundSampleMPEG2File). The unit file includes tests/calllog.cpp itself (it hooks the allocation
# observer). Real beside them: the WAV codecs and the loop (unit w80), the pipe base (w78), the critical sections (w79a) and the
# banks with Bank::CBank (w72, over the bullfrog strings and TbRefRelease). Fakes in the unit file: the decoder CMpegBase and the
# file player, the callback pipes, the channel's SetSampleHeader, GetNextHandle and ReleasePipe, the files, the semaphore and its
# lock, and the clock.
set(TPW_UNIT_SoundCodecFactory src/CCodecFactory.cpp src/TbSoundSampleMPEG2.cpp src/TbSoundCodecs.cpp src/TbSoundPipe.cpp
    src/TbSoundBank.cpp src/snd_handles.cpp src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
set(TPW_UNIT_SoundCodecFactory_OWN_CALLLOG ON)
