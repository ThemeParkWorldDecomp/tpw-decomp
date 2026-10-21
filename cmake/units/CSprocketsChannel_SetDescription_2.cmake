# Unit v51032: CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor, TbMapStreamer
# (Unload, LoadCatgoryFixupTable, SetDroppable) and TbStaticFileBank (src/sound/TbMapStreamer.cpp, src/sound/TimerCodec.cpp).
# The callees outside the unit are recording fakes in tests/unit_CSprocketsChannel_SetDescription_2.cpp.
set(TPW_UNIT_CSprocketsChannel_SetDescription_2 src/sound/TbMapStreamer.cpp src/sound/TimerCodec.cpp platform/arena.cpp)
set(TPW_UNIT_CSprocketsChannel_SetDescription_2_KEEP
  __ZN13SamsUtilities29USoundDescriptionCounter_CopyEPNS_17UReferenceCounterINS_17USoundDescriptionEEEPKS2_   # SamsUtilities::USoundDescriptionCounter_Copy(SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*, SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription> const*)
  __ZN13SamsUtilities33USoundDescriptionCounter_DestructEPNS_17UReferenceCounterINS_17USoundDescriptionEEEs   # SamsUtilities::USoundDescriptionCounter_Destruct(SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*, short)
  __ZN13TbMapStreamer12SetDroppableEb   # TbMapStreamer::SetDroppable(bool)
  __ZN13TbMapStreamer14CCatFixupTable6UnloadEv   # TbMapStreamer::CCatFixupTable::Unload()
  __ZN13TbMapStreamer21LoadCatgoryFixupTableEPN3bfl10FileSystem5CFileE   # TbMapStreamer::LoadCatgoryFixupTable(bfl::FileSystem::CFile*)
  __ZN16TbStaticFileBank10DeregisterEv   # TbStaticFileBank::Deregister()
  __ZN16TbStaticFileBank8DestructEs   # TbStaticFileBank::Destruct(short)
  __ZN16TbStaticFileBankD1Ev   # TbStaticFileBank::~TbStaticFileBank()
  __ZN20CSprocketsChannelMac14SetDescriptionER14TbSampleHeader   # CSprocketsChannelMac::SetDescription(TbSampleHeader&)
)
