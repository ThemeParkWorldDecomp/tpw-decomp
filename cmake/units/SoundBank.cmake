# Unit w72: the sound library's sample banks (TbBank, TbFileBank, TbMMFileBank, TbSoundBankInfo, CBankFactory, Bank::CBank and
# the byte swap helpers), over the bullfrog strings (real) and TbRefRelease (real). The swap helpers do nothing on this
# little-endian host, as in the launcher: the test feeds the host the little-endian words of the file and the emulator the same
# bytes as big-endian words, and compares the values.
set(TPW_UNIT_SoundBank src/TbSoundBank.cpp src/TbSoundInfo.cpp src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp src/CAudioComponent.cpp)
# (src/TbSoundInfo.cpp is unit w77's: TbSoundSampleInfo's constructor, which the SetBank test runs over a record.)
# (src/CAudioComponent.cpp is unit w79b's: include/TbSoundBank.h now takes CAudioComponent from include/TbSoundCore.h, and the
# test's FakeComponent derives from it.)
