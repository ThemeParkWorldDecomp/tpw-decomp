# Unit w79g3: the placeholder subclasses (CPlaceHolderOneShot, ...OneShotSentence, ...OneShotBranchingSentence, CPlaceHolderSentence and its
# Linear, Branching, Shuffle and CListSentence subclasses, with CListShuffle and CListData). Real: the placeholder base CAudioPlaceHolder
# (w79g2) and the one-shot sentence Elements (w79g4), the info records and positions (w77), TbSoundBank.cpp's SetBank and CvtDblUl (w72),
# TbRefRelease (w78), CAudioComponent and the command constructors (w79b). The unit file fakes the sound manager (w79e is not linked:
# its getters, PlaySample, ConstructSample, Rolloff and SystemCommand record), the commands' Execute bodies (w79d), the category
# container, the streamer's Instance and fix-up call, TbEventMapper::MapEvent, bfl::maths::Random and RangedRandomSlow, LbTime_GetClock
# and LbFile_Read and LbFile_Write. The unit file includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_PlaceHolderTypes src/PlaceHolderTypes.cpp src/PlaceHolderHelpers.cpp src/AudioPlaceHolder.cpp src/TbSoundInfo.cpp
    src/TbSoundBank.cpp src/CAudioComponent.cpp src/snd_commands.cpp src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp
    platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_PlaceHolderTypes_OWN_CALLLOG ON)
