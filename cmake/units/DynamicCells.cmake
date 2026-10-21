# Unit w120: the dynamic cell meshes in the frame (DC_TickDynamicCellMeshes, CDynamicMesh::DrawAll, DC_CreatePolyFace; src/game/park/DynamicCell.cpp)
# and the four LLCCM_Create*Mesh instancers (src/game/park/LLCellMouse.cpp). Every callee outside the two files is a fake in tests/unit_DynamicCells.cpp.
set(TPW_UNIT_DynamicCells src/game/park/DynamicCell.cpp src/game/park/LLCellMouse.cpp)
set(TPW_UNIT_DynamicCells_KEEP
  __Z17LLCCM_CreateQMeshiiN9CCellBase14CELL_IndexTypeEi   # LLCCM_CreateQMesh(int, int, CCellBase::CELL_IndexType, int)
  __Z22LLCCM_CreateKTrackMeshiiN9CCellBase14CELL_IndexTypeEi   # LLCCM_CreateKTrackMesh(int, int, CCellBase::CELL_IndexType, int)
  __Z22LLCCM_CreateWTrackMeshiiN9CCellBase14CELL_IndexTypeEi   # LLCCM_CreateWTrackMesh(int, int, CCellBase::CELL_IndexType, int)
  __Z24DC_TickDynamicCellMeshesv   # DC_TickDynamicCellMeshes()
  __Z24LLCCM_CreateHoardingMeshiiN9CCellBase14CELL_IndexTypeEi   # LLCCM_CreateHoardingMesh(int, int, CCellBase::CELL_IndexType, int)
  __ZN12CDynamicMesh17DC_CreatePolyFaceEP8s3DPointiS1_S1_   # CDynamicMesh::DC_CreatePolyFace(s3DPoint*, int, s3DPoint*, s3DPoint*)
  __ZN12CDynamicMesh7DrawAllEv   # CDynamicMesh::DrawAll()
  _gDynamicFaceKeys
  _gDynamicFaces
  _gDynamicFlagSpots
)
