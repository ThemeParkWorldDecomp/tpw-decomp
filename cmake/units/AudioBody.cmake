# Unit w79f: the audio object the game holds (TbAudioBody, CAudioComponentFactory, CreateAudioObject). Real: the component chain
# (CAudioComponent, w79b), TbRefRelease (w78), the info and position records (w77) and the bullfrog strings. The unit file includes
# tests/calllog.cpp itself (it hooks the allocation observer). Fakes there: the sound manager and the core under it, the streamer
# and the event mapper, the placeholder list and placeholders (w79g1, w79g2), a hooked component and a hooked body.
set(TPW_UNIT_AudioBody src/TbAudioBody.cpp src/TbAudioBody_create.cpp src/CAudioComponent.cpp src/TbSoundInfo.cpp src/TbRefRelease.cpp src/TbString.cpp
    src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_AudioBody_OWN_CALLLOG ON)
