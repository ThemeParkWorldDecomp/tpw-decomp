set(TPW_UNIT_NodeGamuts src/game/MeshLoad.cpp src/game/LMatrix.cpp platform/arena.cpp)
set(TPW_UNIT_NodeGamuts_KEEP
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z21DoCalcNodeHFieldGamutP9tag_sNodeP7sHField7sMatrix   # DoCalcNodeHFieldGamut(tag_sNode*, sHField*, sMatrix)
  __Z21SetupHFieldNodeGamutsP15tag_sM3D2Header   # SetupHFieldNodeGamuts(tag_sM3D2Header*)
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
  _gNodeGamutMaxX
  _gNodeGamutMaxZ
  _gNodeGamutMinX
  _gNodeGamutMinZ
)
