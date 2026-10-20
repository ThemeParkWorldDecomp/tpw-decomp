# Unit w79g4: CMapReference, the one-shot sentence children (CPlaceHolderOneShotThrowaway, CPlaceHolderOneShotSentenceElement,
# CPlaceHolderOneShotBranchingSentenceElement), sound::TbAudioEvent's constructor and getters and the CAutoArrayPtr<ListItem> instance
# (the last two are inline in include/). Real: the placeholder base CAudioPlaceHolder (w79g2), the info records and positions (w77),
# TbSoundBank.cpp's SetBank and CvtDblUl (w72), TbRefRelease (w78), CAudioComponent and the command constructors (w79b). The unit file
# fakes the sound manager (w79e is not linked), the commands' Execute bodies (w79d), the category container, the streamer's Instance and
# the fix-up call, bfl::maths::Random and RangedRandomSlow, LbTime_GetClock and LbFile_Read
# and LbFile_Write. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_PlaceHolderHelpers src/PlaceHolderHelpers.cpp src/AudioPlaceHolder.cpp src/TbSoundInfo.cpp src/TbSoundBank.cpp
    src/CAudioComponent.cpp src/snd_commands.cpp src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp
    platform/arena.cpp)
set(TPW_UNIT_PlaceHolderHelpers_OWN_CALLLOG ON)
