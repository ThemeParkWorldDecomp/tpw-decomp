# Unit w79g1: CAudioPlaceHolderList, the pool and the priority list of the sound placeholders. Real: the sound lock and the
# critical section (src/snd_handles.cpp), the info and position records (w77: TbSoundInfo.cpp, with TbRefRelease).
# The unit file fakes the placeholder's own members the list calls (CAudioPlaceHolder is NOT linked: its heavy members need the
# sound manager, the map data and the file helpers, and w79g2's test covers them against the original), the nine subclasses'
# CreateObject statics (unit w79g3), the streamer's Instance and fix-up table calls, the clock and the file object. The unit file
# includes tests/calllog.cpp itself (it hooks the allocation observer).
set(TPW_UNIT_AudioPlaceHolderList src/AudioPlaceHolderList.cpp src/snd_handles.cpp src/TbSoundInfo.cpp src/TbRefRelease.cpp
    platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_AudioPlaceHolderList_OWN_CALLLOG ON)
