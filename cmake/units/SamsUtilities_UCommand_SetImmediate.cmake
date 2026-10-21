# Unit v51107: the small Sound Sprockets pieces of CSprocketsModule.cpp (src/sound/CSprocketsModule.cpp, src/sound/SprocketsChannel.cpp). The callees
# outside the unit are recording fakes in tests/unit_SamsUtilities_UCommand_SetImmediate.cpp.
set(TPW_UNIT_SamsUtilities_UCommand_SetImmediate src/sound/CSprocketsModule.cpp src/sound/SprocketsChannel.cpp
    platform/arena.cpp)
set(TPW_UNIT_SamsUtilities_UCommand_SetImmediate_KEEP
  __Z16TbRect_ConstructP6TbRectiiii   # TbRect_Construct(TbRect*, int, int, int, int)
  __Z17UHasFlags_SetFlagPvj   # UHasFlags_SetFlag(void*, unsigned int)
  __Z19UHasFlags_ClearFlagPvj   # UHasFlags_ClearFlag(void*, unsigned int)
  __Z19UHasFlags_ConstructPv   # UHasFlags_Construct(void*)
  __Z23UHasFlags_ConstructWithPvj   # UHasFlags_ConstructWith(void*, unsigned int)
  __ZN13SamsUtilities12UCommandPlayC1Ev   # SamsUtilities::UCommandPlay::UCommandPlay()
  __ZN13SamsUtilities12UCommandStopC1Ev   # SamsUtilities::UCommandStop::UCommandStop()
  __ZN13SamsUtilities13UCommandPauseC1Ej   # SamsUtilities::UCommandPause::UCommandPause(unsigned int)
  __ZN13SamsUtilities17UReferenceCounted12AddReferenceEv   # SamsUtilities::UReferenceCounted::AddReference()
  __ZN13SamsUtilities18UCommand_ConstructEPNS_8UCommandE   # SamsUtilities::UCommand_Construct(SamsUtilities::UCommand*)
  __ZN13SamsUtilities21UCommandSoundDistanceC1Ed   # SamsUtilities::UCommandSoundDistance::UCommandSoundDistance(double)
  __ZN13SamsUtilities23UCommand_ConstructFlagsEPNS_8UCommandEj   # SamsUtilities::UCommand_ConstructFlags(SamsUtilities::UCommand*, unsigned int)
  __ZN13SamsUtilities29USoundDescriptionCounter_MakeEPNS_17UReferenceCounterINS_17USoundDescriptionEEEPS1_   # SamsUtilities::USoundDescriptionCounter_Make(SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*, SamsUtilities::USoundDescription*)
  __ZN13SamsUtilities8UCommand12SetImmediateEh   # SamsUtilities::UCommand::SetImmediate(unsigned char)
  __ZN13SamsUtilities8UManagerINS_10UAllocatorEEptEv   # SamsUtilities::UManager<SamsUtilities::UAllocator>::operator->()
  __ZN16CSprocketsModule10SetOptionsEj   # CSprocketsModule::SetOptions(unsigned int)
  __ZN16CSprocketsModule21Set3DDSPFunctionalityEjj   # CSprocketsModule::Set3DDSPFunctionality(unsigned int, unsigned int)
  __ZN16CSprocketsModuleD1Ev   # CSprocketsModule::~CSprocketsModule()
  __ZN17CSprocketsSpeakerC1Ev   # CSprocketsSpeaker::CSprocketsSpeaker()
  __ZN20CSprocketsChannelMac14GetDescriptionEv   # CSprocketsChannelMac::GetDescription()
  __ZN20CSprocketsChannelMac4Is3DEv   # CSprocketsChannelMac::Is3D()
  __ZN20CSprocketsChannelMac7SetPipeEPv   # CSprocketsChannelMac::SetPipe(void*)
  __ZN9TbSpeaker15ConfigConstructEPv   # TbSpeaker::ConfigConstruct(void*)
  __ZN9TbSpeaker15ModuleConstructEPv   # TbSpeaker::ModuleConstruct(void*)
  __ZN9TbSpeaker15StereoConstructEPv   # TbSpeaker::StereoConstruct(void*)
  __ZNK13SamsUtilities17UReferenceCounterINS_17USoundDescriptionEEcvPS1_Ev   # SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>::operator SamsUtilities::USoundDescription*() const
  __ZNK13SamsUtilities7UStatus4IfOkE13TbLegacyErrorS1_   # SamsUtilities::UStatus::IfOk(TbLegacyError, TbLegacyError) const
)
