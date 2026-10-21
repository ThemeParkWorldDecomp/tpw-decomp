# Unit v5693: DoMeshSkirtDeform (src/game/park/SkirtDeform.cpp). DoOrSetupSkirtDeform is a recording fake in
# tests/unit_DoMeshSkirtDeform.cpp; SetMatrixIdentity is the real one.
set(TPW_UNIT_DoMeshSkirtDeform src/game/park/SkirtDeform.cpp src/game/LMatrix.cpp platform/arena.cpp)
set(TPW_UNIT_DoMeshSkirtDeform_KEEP
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z17DoMeshSkirtDeformP9tag_sMeshP15tag_sM3D2Headerfj   # DoMeshSkirtDeform(tag_sMesh*, tag_sM3D2Header*, float, unsigned int)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
)
