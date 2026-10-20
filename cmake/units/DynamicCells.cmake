# Unit w120: the dynamic cell meshes in the frame (DC_TickDynamicCellMeshes, CDynamicMesh::DrawAll, DC_CreatePolyFace; src/DynamicCells.cpp)
# and the four LLCCM_Create*Mesh instancers (src/LLCellMeshes.cpp). Every callee outside the two files is a fake in tests/unit_DynamicCells.cpp.
set(TPW_UNIT_DynamicCells src/DynamicCells.cpp src/LLCellMeshes.cpp)
