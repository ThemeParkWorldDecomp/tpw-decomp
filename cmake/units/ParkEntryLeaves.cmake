# The park entry leaves (unit w114): CWorld::DoIHaveTheseRides with the ride database's GetFirstRide and GetNextRide, FLY_RemoveBase with
# DisableRideHead and SCR_KillScript, TPDestroyEmbeddedData, LoadProceduralTextureSet with the height field quads' orientation corrector.
# The arena is the allocator behind the Ptr32 fields of the height field chain. Everything the leaves call outside the set is a fake in
# the unit file.
set(TPW_UNIT_ParkEntryLeaves src/game/World.cpp src/game/rides/RideDB.cpp src/game/rides/FlyingRide.cpp
    src/game/TPEmbed.cpp src/game/Script.cpp src/game/park/TPHField.cpp src/game/MeshLoad.cpp platform/arena.cpp)
set(TPW_UNIT_ParkEntryLeaves_KEEP
  __Z14FLY_RemoveBasei   # FLY_RemoveBase(int)
  __Z14SCR_KillScripti   # SCR_KillScript(int)
  __Z15DisableRideHeadP9tag_sMeshj   # DisableRideHead(tag_sMesh*, unsigned int)
  __Z21TPDestroyEmbeddedDataP9tag_sMesh   # TPDestroyEmbeddedData(tag_sMesh*)
  __Z24LoadProceduralTextureSeti   # LoadProceduralTextureSet(int)
  __Z44CorrectOrientationFlagsForDiffProceduralModei   # CorrectOrientationFlagsForDiffProceduralMode(int)
  __ZN13CRideDatabase11GetNextRideEv   # CRideDatabase::GetNextRide()
  __ZN13CRideDatabase12GetFirstRideEv   # CRideDatabase::GetFirstRide()
  __ZN6CWorld17DoIHaveTheseRidesEPKci   # CWorld::DoIHaveTheseRides(char const*, int)
  _gParkRequiredRides
  _gRidesOwned
)
