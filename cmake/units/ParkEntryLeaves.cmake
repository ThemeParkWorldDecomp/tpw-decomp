# The park entry leaves (unit w114): CWorld::DoIHaveTheseRides with the ride database's GetFirstRide and GetNextRide, FLY_RemoveBase with
# DisableRideHead and SCR_KillScript, TPDestroyEmbeddedData, LoadProceduralTextureSet with the height field quads' orientation corrector.
# The arena is the allocator behind the Ptr32 fields of the height field chain. Everything the leaves call outside the set is a fake in
# the unit file.
set(TPW_UNIT_ParkEntryLeaves src/World_rides.cpp src/RideDB_walk.cpp src/hlsprite_flybase.cpp src/TPEmbed_destroy.cpp src/analyser_kill.cpp
    src/TPRide_proctex.cpp src/rsse_proctex.cpp platform/arena.cpp)
