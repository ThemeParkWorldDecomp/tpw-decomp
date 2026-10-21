set(TPW_UNIT_MeshFrame src/engine/DirectDraw.cpp src/engine/Mesh.cpp src/engine/RenderList.cpp platform/arena.cpp)
set(TPW_UNIT_MeshFrame_KEEP
  __Z10DotProduct10_D3DVECTORS_   # DotProduct(_D3DVECTOR, _D3DVECTOR)
  __Z13ColorVerticesP11SFVF_TLCTEXjii   # ColorVertices(SFVF_TLCTEX*, unsigned int, int, int)
  __ZN5CMesh10PreprocessEP9tag_sNodeP7CMatrixS3_   # CMesh::Preprocess(tag_sNode*, CMatrix*, CMatrix*)
  __ZN5CMesh18PreprocessTexturesEP9tag_sNode   # CMesh::PreprocessTextures(tag_sNode*)
  __ZN5CMesh21StaticCalculateLightsEP9tag_sNodeR7CMatrixPP6CLighti   # CMesh::StaticCalculateLights(tag_sNode*, CMatrix&, CLight**, int)
  __ZN5CMesh7PrepareEv   # CMesh::Prepare()
)
