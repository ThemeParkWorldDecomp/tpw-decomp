# Unit v5308: RATick and QuickCopyUnmaskedMeshVertices (src/game/rides/RideAnim.cpp). The callees outside the unit are recording fakes in
# tests/unit_RATick.cpp, which also defines gTPGlobals.
set(TPW_UNIT_RATick src/game/rides/RideAnim.cpp platform/arena.cpp)
set(TPW_UNIT_RATick_KEEP
  __Z29QuickCopyUnmaskedMeshVerticesP9tag_sMeshS0_i   # QuickCopyUnmaskedMeshVertices(tag_sMesh*, tag_sMesh*, int)
  __Z6RATickP16sAnimTimeControlP9tag_sMeshP15tag_sAnimHeader   # RATick(sAnimTimeControl*, tag_sMesh*, tag_sAnimHeader*)
)
