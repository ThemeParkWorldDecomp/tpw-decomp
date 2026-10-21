# Unit v51014: TpBody::Load/Save, Sound Sprockets command constructors, UReferenceCounted::RemoveReference (src/sound/CSprocketsModule.cpp, src/sound/SprocketsChannel.cpp, src/sound/TpBody.cpp).
set(TPW_UNIT_TpBody_Load src/sound/CSprocketsModule.cpp src/sound/SprocketsChannel.cpp src/sound/TpBody.cpp
    platform/arena.cpp)
set(TPW_UNIT_TpBody_Load_KEEP
  __ZN13SamsUtilities14UCommandVolumeC1Ed   # SamsUtilities::UCommandVolume::UCommandVolume(double)
  __ZN13SamsUtilities15UCommandCPULoadC1Ed   # SamsUtilities::UCommandCPULoad::UCommandCPULoad(double)
  __ZN13SamsUtilities17UCommandSoundTypeC1ENS_9SoundTypeE   # SamsUtilities::UCommandSoundType::UCommandSoundType(SamsUtilities::SoundType)
  __ZN13SamsUtilities17UReferenceCounted15RemoveReferenceEv   # SamsUtilities::UReferenceCounted::RemoveReference()
  __ZN13SamsUtilities17UReferenceCountedD1Ev   # SamsUtilities::UReferenceCounted::~UReferenceCounted()
  __ZN13SamsUtilities19UCommandSpeakerTypeC1ENS_11SpeakerTypeEi   # SamsUtilities::UCommandSpeakerType::UCommandSpeakerType(SamsUtilities::SpeakerType, int)
  __ZN13SamsUtilities21UCommandListenerScaleC1Eddd   # SamsUtilities::UCommandListenerScale::UCommandListenerScale(double, double, double)
  __ZNK6TpBody4LoadEPvh   # TpBody::Load(void*, unsigned char) const
  __ZNK6TpBody4SaveEPv   # TpBody::Save(void*) const
)
