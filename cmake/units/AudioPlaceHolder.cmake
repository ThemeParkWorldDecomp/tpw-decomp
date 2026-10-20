# Unit w79g2: CAudioPlaceHolder, the base of every playing sound (with CDistPriority, CAudioPlaceHolderExtraData and TbSndCallback).
# Real: the info records and positions (w77), TbSoundSampleInfo::SetBank and CvtDblUl (w72: TbSoundBank.cpp, with its strings and
# TbRefRelease), CAudioComponent and the command constructors (w79b). The unit file fakes the sound manager (w79e is not linked:
# TbSoundCore's members and the manager's getters and virtuals are recording fakes), the commands' Execute bodies (w79d), the
# category container, the streamer's Instance and the fix-up call, CMapReference and the two sentence elements (w79g4),
# bfl::maths::Random and RangedRandomSlow, LbTime_GetClock and LbFile_Read and LbFile_Write. The unit file includes
# tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_AudioPlaceHolder src/AudioPlaceHolder.cpp src/TbSoundInfo.cpp src/TbSoundBank.cpp src/CAudioComponent.cpp
    src/snd_commands.cpp src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_AudioPlaceHolder_OWN_CALLLOG ON)
