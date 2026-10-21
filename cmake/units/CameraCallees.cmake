set(TPW_UNIT_CameraCallees src/game/TPCamera.cpp src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_CameraCallees_KEEP
  __Z12SetCameraPOIPfS_S_S_S_   # SetCameraPOI(float*, float*, float*, float*, float*)
  __Z17GetNextGridsquareP8s3DPointS0_S0_   # GetNextGridsquare(s3DPoint*, s3DPoint*, s3DPoint*)
  __Z18RideRideIfRequiredP8s3DPoint   # RideRideIfRequired(s3DPoint*)
  __Z20MatrixDifferenceTinyP7sMatrixS0_   # MatrixDifferenceTiny(sMatrix*, sMatrix*)
  __Z21DetermineHFieldHeightffi   # DetermineHFieldHeight(float, float, int)
  __Z22AllowFullCameraZoomoutv   # AllowFullCameraZoomout()
  __Z24ApplyHeadsOnRidesBumpageP7sMatrixS0_fS0_   # ApplyHeadsOnRidesBumpage(sMatrix*, sMatrix*, float, sMatrix*)
  __Z26DetermineHFieldHeightQuickff   # DetermineHFieldHeightQuick(float, float)
  __Z31RoundRotationToNearest90Degreesv   # RoundRotationToNearest90Degrees()
  _gCameraMatrixEpsilon
  _gCameraPOIValue
  _gGridEdgeOffset
  _gGridNudge
  _gGridStepScale
)
