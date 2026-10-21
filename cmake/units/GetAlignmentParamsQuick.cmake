# Unit v5970: the coaster Bezier and alignment helpers (src/game/rides/CoasterSpline.cpp). The callees outside the unit are
# recording fakes in tests/unit_GetAlignmentParamsQuick.cpp, which also defines gTPGlobals.
set(TPW_UNIT_GetAlignmentParamsQuick src/game/rides/CoasterSpline.cpp platform/arena.cpp)
set(TPW_UNIT_GetAlignmentParamsQuick_KEEP
  __Z13BezierTangentP18tag_sBezierControlS0_fP8s3DPoint   # BezierTangent(tag_sBezierControl*, tag_sBezierControl*, float, s3DPoint*)
  __Z13GetBankParamsP8s3DPointP18tag_sBezierControlS2_fP19sCoasterOrientation   # GetBankParams(s3DPoint*, tag_sBezierControl*, tag_sBezierControl*, float, sCoasterOrientation*)
  __Z14BezierPositionP18tag_sBezierControlS0_fP8s3DPoint   # BezierPosition(tag_sBezierControl*, tag_sBezierControl*, float, s3DPoint*)
  __Z22GetAlignmentParamsSlowP19sCoasterTrackParamsP8s3DPointfP19sCoasterOrientationiPf   # GetAlignmentParamsSlow(sCoasterTrackParams*, s3DPoint*, float, sCoasterOrientation*, int, float*)
  __Z23GetAlignmentParamsQuick8s3DPointfP19sCoasterOrientationPf   # GetAlignmentParamsQuick(s3DPoint, float, sCoasterOrientation*, float*)
)
