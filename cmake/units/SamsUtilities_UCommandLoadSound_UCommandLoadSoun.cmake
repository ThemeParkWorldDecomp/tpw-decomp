# Unit v5758: SamsUtilities::UCommandLoadSound constructor and destructor. The callees are recording fakes in the unit's cpp.
set(TPW_UNIT_SamsUtilities_UCommandLoadSound_UCommandLoadSoun src/sound/SprocketsChannel.cpp platform/arena.cpp)
set(TPW_UNIT_SamsUtilities_UCommandLoadSound_UCommandLoadSoun_KEEP
  __ZN13SamsUtilities17UCommandLoadSound8DestructEs   # SamsUtilities::UCommandLoadSound::Destruct(short)
  __ZN13SamsUtilities17UCommandLoadSoundC1ERKNS_17UReferenceCounterINS_17USoundDescriptionEEE   # SamsUtilities::UCommandLoadSound::UCommandLoadSound(SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription> const&)
)
