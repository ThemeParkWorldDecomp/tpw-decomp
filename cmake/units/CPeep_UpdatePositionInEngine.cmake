# Unit v5974: CPeep::UpdatePositionInEngine and CPeep::PeepWalks (src/game/people/Peep.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPeep_UpdatePositionInEngine.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPeep_UpdatePositionInEngine src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_UpdatePositionInEngine_KEEP
  __ZN5CPeep22UpdatePositionInEngineEf   # CPeep::UpdatePositionInEngine(float)
  __ZN5CPeep9PeepWalksEv   # CPeep::PeepWalks()
  _gPeepBalloonBase
  _gPeepBalloonDivisor
  _gPeepBalloonIndex
  _gPeepBalloonOffsets
  _gPeepBalloonRate
  _gPeepBalloonRows
  _gPeepBalloonScale
  _gPeepBalloonTime
  _gPeepC474
  _gPeepGuardC460
  _gPeepGuardC468
  _gPeepGuardC470
  _gPeepGuardC478
  _gPeepGuardC480
  _gPeepGuardC488
  _gPeepGuardC490
  _gPeepScriptHeight
  _gPeepStepLag
)
