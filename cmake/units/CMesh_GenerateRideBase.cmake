# Unit v5752: CMesh::CenterMatrix and the two CMesh::GenerateRideBase (src/engine/Mesh.cpp). The callees outside the unit are
# recording fakes in tests/unit_CMesh_GenerateRideBase.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMesh_GenerateRideBase src/engine/Mesh.cpp platform/arena.cpp)
set(TPW_UNIT_CMesh_GenerateRideBase_KEEP
  __ZN5CMesh12CenterMatrixEif   # CMesh::CenterMatrix(int, float)
  __ZN5CMesh16GenerateRideBaseEP13CTextureEntryS1_iii   # CMesh::GenerateRideBase(CTextureEntry*, CTextureEntry*, int, int, int)
  __ZN5CMesh16GenerateRideBaseEiiii   # CMesh::GenerateRideBase(int, int, int, int)
)
