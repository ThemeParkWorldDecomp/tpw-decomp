set(TPW_UNIT_RideInstance src/engine/EngineUtil.cpp src/game/Debug.cpp src/game/TPEmbed.cpp src/game/TPInstance.cpp
    src/game/rides/RideAnim.cpp src/game/rides/RidePlacement.cpp src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_RideInstance_KEEP
  __Z11GetFontPathPc   # GetFontPath(char*)
  __Z12AlignTo32BitRPvPj   # AlignTo32Bit(void*&, unsigned int*)
  __Z12AlignTo32BitRjPj   # AlignTo32Bit(unsigned int&, unsigned int*)
  __Z12InstanceAnimP15tag_sAnimHeaderP9sHeapInfojPS0_   # InstanceAnim(tag_sAnimHeader*, sHeapInfo*, unsigned int, tag_sAnimHeader**)
  __Z12InstanceMeshP9tag_sMeshP9sHeapInfojPS0_j   # InstanceMesh(tag_sMesh*, sHeapInfo*, unsigned int, tag_sMesh**, unsigned int)
  __Z14SetupRideHeadsRP9tag_sMesh   # SetupRideHeads(tag_sMesh*&)
  __Z17CorrectFaceFlags1P9tag_sMeshi   # CorrectFaceFlags1(tag_sMesh*, int)
  __Z17CorrectNodeFlags2P16tag_sRideAnimSet   # CorrectNodeFlags2(tag_sRideAnimSet*)
  __Z19InstanceRideAnimSetP16tag_sRideAnimSetP9sHeapInfojPS0_j   # InstanceRideAnimSet(tag_sRideAnimSet*, sHeapInfo*, unsigned int, tag_sRideAnimSet**, unsigned int)
  __Z22DisableDataNodesRenderRP9tag_sMeshj   # DisableDataNodesRender(tag_sMesh*&, unsigned int)
  __Z22InstanceDynamicMeshSetP13tag_sDMeshSetP9sHeapInfojPS0_   # InstanceDynamicMeshSet(tag_sDMeshSet*, sHeapInfo*, unsigned int, tag_sDMeshSet**)
  __Z22OverrideActiveChannelsP16tag_sRideAnimSet   # OverrideActiveChannels(tag_sRideAnimSet*)
  __Z28DisableDataNodesRenderExceptRP9tag_sMeshj   # DisableDataNodesRenderExcept(tag_sMesh*&, unsigned int)
  __Z8ERRORBoxPKcz   # ERRORBox(char const*, ...)
  _gDefaultHeap
  _gPolySearchCount
)
