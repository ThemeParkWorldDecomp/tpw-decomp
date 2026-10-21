set(TPW_UNIT_CameraMatrix src/game/TPCamera.cpp src/game/LMatrix.cpp)
set(TPW_UNIT_CameraMatrix_KEEP
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z17SetExternalCamerai   # SetExternalCamera(int)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z20HandleExternalCamerav   # HandleExternalCamera()
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
  _gCurrentCameraMatrix
  _gExternalCameraMatrix
  _gFirstPersonFlags
  _gInverseCameraMatrix
)
