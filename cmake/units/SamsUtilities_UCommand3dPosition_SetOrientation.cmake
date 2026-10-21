# Unit v51053: the position commands, GetTag answers, speaker kinds and static initialiser of CSprocketsModule.cpp
# (src/sound/CSprocketsModule.cpp). The callees outside the unit
# are recording fakes in tests/unit_SamsUtilities_UCommand3dPosition_SetOrientation.cpp, which also defines the globals of other files.
set(TPW_UNIT_SamsUtilities_UCommand3dPosition_SetOrientation src/sound/CSprocketsModule.cpp platform/arena.cpp)
set(TPW_UNIT_SamsUtilities_UCommand3dPosition_SetOrientation_KEEP
  __Z28__sinit_CSprocketsModule_cppv   # __sinit_CSprocketsModule_cpp()
  __ZN13SamsUtilities12UCommandPlay6GetTagEv   # SamsUtilities::UCommandPlay::GetTag()
  __ZN13SamsUtilities12UCommandStop6GetTagEv   # SamsUtilities::UCommandStop::GetTag()
  __ZN13SamsUtilities13UCommandPause6GetTagEv   # SamsUtilities::UCommandPause::GetTag()
  __ZN13SamsUtilities14UCommandVolume6GetTagEv   # SamsUtilities::UCommandVolume::GetTag()
  __ZN13SamsUtilities15UCommandCPULoad6GetTagEv   # SamsUtilities::UCommandCPULoad::GetTag()
  __ZN13SamsUtilities15UCommandPreroll6GetTagEv   # SamsUtilities::UCommandPreroll::GetTag()
  __ZN13SamsUtilities15UCommandPrerollC1Ej   # SamsUtilities::UCommandPreroll::UCommandPreroll(unsigned int)
  __ZN13SamsUtilities16UCommandPosition6GetTagEv   # SamsUtilities::UCommandPosition::GetTag()
  __ZN13SamsUtilities16UCommandPositionC1Ej   # SamsUtilities::UCommandPosition::UCommandPosition(unsigned int)
  __ZN13SamsUtilities17UCommandLoadSound6GetTagEv   # SamsUtilities::UCommandLoadSound::GetTag()
  __ZN13SamsUtilities17UCommandSoundType6GetTagEv   # SamsUtilities::UCommandSoundType::GetTag()
  __ZN13SamsUtilities18UCommand3dPosition11SetLocationEddd   # SamsUtilities::UCommand3dPosition::SetLocation(double, double, double)
  __ZN13SamsUtilities18UCommand3dPosition11SetUpVectorEddd   # SamsUtilities::UCommand3dPosition::SetUpVector(double, double, double)
  __ZN13SamsUtilities18UCommand3dPosition11SetVelocityEddd   # SamsUtilities::UCommand3dPosition::SetVelocity(double, double, double)
  __ZN13SamsUtilities18UCommand3dPosition14SetOrientationEddd   # SamsUtilities::UCommand3dPosition::SetOrientation(double, double, double)
  __ZN13SamsUtilities18UCommand3dPositionC1Ej   # SamsUtilities::UCommand3dPosition::UCommand3dPosition(unsigned int)
  __ZN13SamsUtilities19UCommandSpeakerType6GetTagEv   # SamsUtilities::UCommandSpeakerType::GetTag()
  __ZN13SamsUtilities21UCommandListenerScale6GetTagEv   # SamsUtilities::UCommandListenerScale::GetTag()
  __ZN13SamsUtilities21UCommandSoundDistance6GetTagEv   # SamsUtilities::UCommandSoundDistance::GetTag()
  __ZN13SamsUtilities24UCommandListenerPosition6GetTagEv   # SamsUtilities::UCommandListenerPosition::GetTag()
  __ZN13SamsUtilities24UCommandListenerPositionC1Ej   # SamsUtilities::UCommandListenerPosition::UCommandListenerPosition(unsigned int)
  __ZN13SamsUtilities7UStatusC1Ev   # SamsUtilities::UStatus::UStatus()
  __ZN13SamsUtilities7UStatusaSERKS0_   # SamsUtilities::UStatus::operator=(SamsUtilities::UStatus const&)
  __ZN16CSprocketsModule15SetSpeedOfSoundEf   # CSprocketsModule::SetSpeedOfSound(float)
  __ZN16CSprocketsModule19SetListenerVelocityERK15TbSoundPosition   # CSprocketsModule::SetListenerVelocity(TbSoundPosition const&)
  __ZNK13SamsUtilities7UStatus5IsSetEv   # SamsUtilities::UStatus::IsSet() const
  __ZNK17CSprocketsSpeaker10HeadphonesEv   # CSprocketsSpeaker::Headphones() const
  __ZNK17CSprocketsSpeaker4MonoEv   # CSprocketsSpeaker::Mono() const
  __ZNK17CSprocketsSpeaker6StereoEv   # CSprocketsSpeaker::Stereo() const
  _gSinit_1003f3b8
)
