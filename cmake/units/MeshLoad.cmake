set(TPW_UNIT_MeshLoad src/game/MeshLoad.cpp src/game/TPMesh.cpp platform/arena.cpp)
set(TPW_UNIT_MeshLoad_KEEP
  __Z11GetMeshPathv   # GetMeshPath()
  __Z15SetNewMeshPathsPcS_S_S_   # SetNewMeshPaths(char*, char*, char*, char*)
  __Z16CreateEngineMeshP9tag_sMesh   # CreateEngineMesh(tag_sMesh*)
  __Z17DestroyPathAllocsv   # DestroyPathAllocs()
  __Z18GetFileAccessIndexv   # GetFileAccessIndex()
  __Z18SetFileAccessIndexi   # SetFileAccessIndex(int)
  __Z20OverrideTexturePathsP15tag_sM3D2Headerii   # OverrideTexturePaths(tag_sM3D2Header*, int, int)
  __Z22BuildSharedTexturePathPc   # BuildSharedTexturePath(char*)
  __Z24CreateMeshFromM3D2HeaderP15tag_sM3D2Header   # CreateMeshFromM3D2Header(tag_sM3D2Header*)
  __Z24CreateMeshTypeEngineMeshP9tag_sMesh   # CreateMeshTypeEngineMesh(tag_sMesh*)
  __Z8LoadMeshPcPviii   # LoadMesh(char*, void*, int, int, int)
  _gFileAccessIndex
  _gMeshPathAllocs
  _gMeshPathCount
  _gMeshPathIndex
  _gMeshPaths
  _gSharedPathIndex
  _gTexturePathIndex
)
