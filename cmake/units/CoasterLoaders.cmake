# Unit w148: the coaster type's remaining loaders (CreateTrackSectionMesh, SetupTrainType, SetupCarType, LoadCoasterPylons,
# SetupPylonData, the cross-section selects, OptimiseCrossSections, RepackCrossSection, GetTempFixedRideParams; src/game/rides/CoasterPylon.cpp, src/game/rides/CoasterType.cpp).
# The callees outside the unit are recording fakes in tests/unit_CoasterLoaders.cpp, which also defines the globals of other files.
# List only the sources the library must compile.
set(TPW_UNIT_CoasterLoaders src/game/rides/CoasterPylon.cpp src/game/rides/CoasterType.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterLoaders_KEEP
  __Z12SetupCarTypeP8sCarTypePc   # SetupCarType(sCarType*, char*)
  __Z14SetupPylonDataP16tag_sCoasterTypeP13sPylonControlj   # SetupPylonData(tag_sCoasterType*, sPylonControl*, unsigned int)
  __Z14SetupTrainTypeP16tag_sCoasterTypePc   # SetupTrainType(tag_sCoasterType*, char*)
  __Z17LoadCoasterPylonsP16tag_sCoasterTypeP13sPylonControlS2_Pc   # LoadCoasterPylons(tag_sCoasterType*, sPylonControl*, sPylonControl*, char*)
  __Z18RepackCrossSectionPjP14sCoasterRecordP18sCrossSectionPoint   # RepackCrossSection(unsigned int*, sCoasterRecord*, sCrossSectionPoint*)
  __Z21OptimiseCrossSectionsP16tag_sCoasterTypeP14sCoasterRecordj   # OptimiseCrossSections(tag_sCoasterType*, sCoasterRecord*, unsigned int)
  __Z22CreateTrackSectionMeshP16tag_sCoasterType   # CreateTrackSectionMesh(tag_sCoasterType*)
  __Z22GetTempFixedRideParamsiiii   # GetTempFixedRideParams(int, int, int, int)
  __Z29SetCrossSectionSelectDefaultsP16tag_sCoasterTypejPP23tag_sCrossSectionSelectP14sCoasterRecordS5_i   # SetCrossSectionSelectDefaults(tag_sCoasterType*, unsigned int, tag_sCrossSectionSelect**, sCoasterRecord*, sCoasterRecord*, int)
  __Z36CreateCrossSectionSelectsFromSamFileP16tag_sCoasterTypeP14sCoasterRecordjP15CCoasterBalancei   # CreateCrossSectionSelectsFromSamFile(tag_sCoasterType*, sCoasterRecord*, unsigned int, CCoasterBalance*, int)
)
