set(TPW_UNIT_RideDestroy src/game/DynamicMeshSet.cpp src/game/TPInstance.cpp src/game/TPMesh.cpp
    src/game/park/SkirtDeform.cpp src/game/park/TPHField.cpp src/game/rides/TPRide.cpp src/game/rides/TPCoaster.cpp
    platform/arena.cpp)
set(TPW_UNIT_RideDestroy_KEEP
  __Z10DMFreeMeshP13tag_sDMeshSet   # DMFreeMesh(tag_sDMeshSet*)
  __Z11DestroyMeshP9tag_sMesh   # DestroyMesh(tag_sMesh*)
  __Z15DestroyRideTypej   # DestroyRideType(unsigned int)
  __Z17DestroyMeshHeaderP15tag_sM3D2Header   # DestroyMeshHeader(tag_sM3D2Header*)
  __Z18DestroyRideAnimSetP16tag_sRideAnimSeti   # DestroyRideAnimSet(tag_sRideAnimSet*, int)
  __Z19DMDisableMeshRenderP13tag_sDMeshSet   # DMDisableMeshRender(tag_sDMeshSet*)
  __Z19DMDisableMeshRenderP13tag_sDMeshSet8HFieldXZS1_   # DMDisableMeshRender(tag_sDMeshSet*, HFieldXZ, HFieldXZ)
  __Z20DestroyRideHeightMapP16tag_sRideAnimSet   # DestroyRideHeightMap(tag_sRideAnimSet*)
  __Z20DestroySkirtVerticesP16tag_sRideAnimSet   # DestroySkirtVertices(tag_sRideAnimSet*)
  __Z22DestroyCoasterInstanceP20tag_sCoasterInstance   # DestroyCoasterInstance(tag_sCoasterInstance*)
  __Z23DestroyCoasterInstancesv   # DestroyCoasterInstances()
  __Z26DestroyRideAnimSetInstanceP16tag_sRideAnimSetP9sHeapInfoj   # DestroyRideAnimSetInstance(tag_sRideAnimSet*, sHeapInfo*, unsigned int)
  __Z36DestroyCoasterInstanceFromEntranceIDj   # DestroyCoasterInstanceFromEntranceID(unsigned int)
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
