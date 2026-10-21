# Unit w138: the six coaster type loaders (CreateCoasterFromSamFile and its five builders) are real; the allocator, ERRORBox and
# the four cross-section builders are recording fakes in tests/unit_CoasterTypes.cpp.
set(TPW_UNIT_CoasterTypes src/game/rides/CoasterType.cpp)
set(TPW_UNIT_CoasterTypes_KEEP
  __Z22SetCoasterTypeDefaultsP16tag_sCoasterType   # SetCoasterTypeDefaults(tag_sCoasterType*)
  __Z24CreateCoasterFromSamFileP16tag_sCoasterTypePcP15CCoasterBalance   # CreateCoasterFromSamFile(tag_sCoasterType*, char*, CCoasterBalance*)
  __Z29SetupPylonControlsFromSamFileP16tag_sCoasterTypeP15CCoasterBalance   # SetupPylonControlsFromSamFile(tag_sCoasterType*, CCoasterBalance*)
  __Z30CreateCrossSectionsFromSamFileP16tag_sCoasterTypeP15CCoasterBalance   # CreateCrossSectionsFromSamFile(tag_sCoasterType*, CCoasterBalance*)
  __Z32CreateCoasterTrackMasterDMeshSetP16tag_sCoasterTypePcP15CCoasterBalance   # CreateCoasterTrackMasterDMeshSet(tag_sCoasterType*, char*, CCoasterBalance*)
  __Z32SetupCarAndTrainTypesFromSamFileP16tag_sCoasterTypeP15CCoasterBalance   # SetupCarAndTrainTypesFromSamFile(tag_sCoasterType*, CCoasterBalance*)
  _gCoasterDefaultPairs
  _gCoasterStaticTextureEntries
)
