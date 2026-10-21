# Unit w112b: the dynamic cell meshes (CDynamicMesh, DMCreateMesh, DMEnableMeshRender, CorrectDTexturePaths, LLCC_SystemGetMode). The arena is needed for
# the Ptr32 fields of the mesh block (as in RideDestroy and RideAnimInterp). Every callee outside the file is a fake in tests/unit_DynamicMesh.cpp.
set(TPW_UNIT_DynamicMesh src/game/DynamicMeshSet.cpp src/game/MeshLoad.cpp src/game/park/DynamicCell.cpp
    src/game/park/LLCellSystem.cpp platform/arena.cpp)
set(TPW_UNIT_DynamicMesh_KEEP
  __Z12DMCreateMeshP13tag_sDMeshSetP12sDTextureSetPKcj   # DMCreateMesh(tag_sDMeshSet*, sDTextureSet*, char const*, unsigned int)
  __Z18DMEnableMeshRenderP13tag_sDMeshSet   # DMEnableMeshRender(tag_sDMeshSet*)
  __Z18DMEnableMeshRenderP13tag_sDMeshSet8HFieldXZS1_   # DMEnableMeshRender(tag_sDMeshSet*, HFieldXZ, HFieldXZ)
  __Z18LLCC_SystemGetModev   # LLCC_SystemGetMode()
  __Z20CorrectDTexturePathsP12sDTextureSetP13tag_sDMeshSet   # CorrectDTexturePaths(sDTextureSet*, tag_sDMeshSet*)
  __ZN12CDynamicMesh12DC_SetVertexEfffffiP8s3DPoint   # CDynamicMesh::DC_SetVertex(float, float, float, float, float, int, s3DPoint*)
  __ZN12CDynamicMesh14SetBoundingBoxEffffff   # CDynamicMesh::SetBoundingBox(float, float, float, float, float, float)
  __ZN12CDynamicMesh19DC_CreateSquareFaceEfffhhifihi   # CDynamicMesh::DC_CreateSquareFace(float, float, float, unsigned char, unsigned char, int, float, int, unsigned char, int)
  __ZN12CDynamicMeshC1EttPjPKc8DYN_Typeffffff   # CDynamicMesh::CDynamicMesh(unsigned short, unsigned short, unsigned int*, char const*, DYN_Type, float, float, float, float, float, float)
  __ZN12CDynamicMeshD1Ev   # CDynamicMesh::~CDynamicMesh()
  _gDynamicCellF9BC0
  _gDynamicCellF9BC4
  _gDynamicCellTexturePath
  _gDynamicCellUV
  _gLLCCDB50
  _gLLCCDB54
)
