# Unit v51075: small sound functions (src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp) and the existing TbSoundSampleStore::operator[].
# The callees outside the unit are recording fakes in tests/unit_TbSpeaker_Mono_operator_int.cpp.
set(TPW_UNIT_TbSpeaker_Mono_operator_int src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp src/TbSoundSampleStore.cpp platform/arena.cpp)
