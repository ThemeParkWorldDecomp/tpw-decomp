# Unit v5250: the CSprocketsModule channel commands (src/sound/CSprocketsModule.cpp). The callees outside
# the unit are recording fakes in tests/unit_CSprocketsModule_SetChannelVolume.cpp.
set(TPW_UNIT_CSprocketsModule_SetChannelVolume src/sound/CSprocketsModule.cpp platform/arena.cpp)
set(TPW_UNIT_CSprocketsModule_SetChannelVolume_KEEP
  __ZN16CSprocketsModule11GetPositionEjR15TbSoundPosition   # CSprocketsModule::GetPosition(unsigned int, TbSoundPosition&)
  __ZN16CSprocketsModule11StopChannelEj   # CSprocketsModule::StopChannel(unsigned int)
  __ZN16CSprocketsModule12PauseChannelEj   # CSprocketsModule::PauseChannel(unsigned int)
  __ZN16CSprocketsModule16SetChannelVolumeEjR17TbSoundSampleInfo   # CSprocketsModule::SetChannelVolume(unsigned int, TbSoundSampleInfo&)
  __ZN16CSprocketsModule16SetSpeakerConfigERKN9TbSpeaker6ConfigE   # CSprocketsModule::SetSpeakerConfig(TbSpeaker::Config const&)
  __ZN16CSprocketsModule19SetListenerPositionERK15TbSoundPosition   # CSprocketsModule::SetListenerPosition(TbSoundPosition const&)
  __ZN16CSprocketsModule7StopAllEv   # CSprocketsModule::StopAll()
  __ZN16CSprocketsModule8PauseAllEv   # CSprocketsModule::PauseAll()
)
