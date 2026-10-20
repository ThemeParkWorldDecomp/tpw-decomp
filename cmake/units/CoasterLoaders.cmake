# Unit w148: the coaster type's remaining loaders (CreateTrackSectionMesh, SetupTrainType, SetupCarType, LoadCoasterPylons,
# SetupPylonData, the cross-section selects, OptimiseCrossSections, RepackCrossSection, GetTempFixedRideParams; src/TPCoaster_loaders.cpp).
# The callees outside the unit are recording fakes in tests/unit_CoasterLoaders.cpp, which also defines the globals of other files.
# List only the sources the library must compile.
set(TPW_UNIT_CoasterLoaders src/TPCoaster_loaders.cpp platform/arena.cpp)
