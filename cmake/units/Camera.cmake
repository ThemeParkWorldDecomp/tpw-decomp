set(TPW_UNIT_Camera src/game/TPCamera.cpp src/game/LMatrix.cpp platform/arena.cpp)
set(TPW_UNIT_Camera_KEEP
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z12HandleCamerav   # HandleCamera()
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z17SetExternalCamerai   # SetExternalCamera(int)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z20HandleExternalCamerav   # HandleExternalCamera()
  __Z21XRotateLMatrixNoTransP7sMatrixff   # XRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z21YRotateLMatrixNoTransP7sMatrixff   # YRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z21ZRotateLMatrixNoTransP7sMatrixff   # ZRotateLMatrixNoTrans(sMatrix*, float, float)
  __Z23RotateLMatrixXYZNoTransP7sMatrix13eRotateParamsPf   # RotateLMatrixXYZNoTrans(sMatrix*, eRotateParams, float*)
  __Z23SetLMatrixOrientationHiP7sMatrixff   # SetLMatrixOrientationHi(sMatrix*, float, float)
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z25RotateLMatrixXYZNoTransHiP7sMatrix13eRotateParamsPf   # RotateLMatrixXYZNoTransHi(sMatrix*, eRotateParams, float*)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
  _gCameraF59FC
  _gCameraGroundHeight
  _gCameraPitch
  _gCameraPitchMax
  _gCameraPitchMin
  _gCameraPointerAnchor
  _gCameraStopSpeed
  _gCameraTilt
  _gCameraTiltMaxX
  _gCameraTiltMaxZ
  _gCameraTiltMinX
  _gCameraTiltMinZ
  _gCameraWalkOnce
  _gCameraWideZoomMax
  _gCameraWideZoomMaxOnce
  _gCameraWideZoomMin
  _gCameraWideZoomOnce
  _gCameraYaw
  _gCameraYawRate
  _gCameraYawRateIso
  _gCameraYawSnap
  _gCurrentCameraMatrix
  _gExternalCameraMatrix
  _gFirstPersonFlags
  _gFirstPersonPitch
  _gFirstPersonRideMatrix
  _gFirstPersonTurn
  _gInverseCameraMatrix
)
