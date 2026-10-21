# Unit v5L20: the coaster collision system (src/game/rides/CoasterCollision.cpp, src/game/rides/CoasterPylon.cpp). The callees outside the unit (NormaliseVector,
# MReleaseAllocF, MReleaseFreeF) are recording fakes in tests/unit_CoasterCollision.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterCollision src/game/rides/CoasterCollision.cpp src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterCollision_KEEP
  __Z12DotProduct3DPK8s3DPointS1_   # DotProduct3D(s3DPoint const*, s3DPoint const*)
  __Z14GetPylonAtCelljjPj   # GetPylonAtCell(unsigned int, unsigned int, unsigned int*)
  __Z14GetTrackBoundsP20tag_sCoasterInstanceP15tag_sTrackBraceS2_P11sAlignedBox   # GetTrackBounds(tag_sCoasterInstance*, tag_sTrackBrace*, tag_sTrackBrace*, sAlignedBox*)
  __Z17RecalcTrackBoundsP20tag_sCoasterInstance   # RecalcTrackBounds(tag_sCoasterInstance*)
  __Z19DoCollisionCheckingP20tag_sCoasterInstance   # DoCollisionChecking(tag_sCoasterInstance*)
  __Z22DoProperChunkCollisionP11sTrackChunkS0_   # DoProperChunkCollision(sTrackChunk*, sTrackChunk*)
  __Z26GenerateSamplesInBoundAreaP20tag_sCoasterInstanceP15tag_sTrackBraceS2_PK11sAlignedBoxP11sTrackChunkPj   # GenerateSamplesInBoundArea(tag_sCoasterInstance*, tag_sTrackBrace*, tag_sTrackBrace*, sAlignedBox const*, sTrackChunk*, unsigned int*)
  __Z27CheckTrackToTrackCollisionsP20tag_sCoasterInstanceS0_P15tag_sTrackBraceS2_S2_S2_   # CheckTrackToTrackCollisions(tag_sCoasterInstance*, tag_sCoasterInstance*, tag_sTrackBrace*, tag_sTrackBrace*, tag_sTrackBrace*, tag_sTrackBrace*)
  __Z28RemoveAllCollisionsFromBraceP20tag_sCoasterInstanceP15tag_sTrackBrace   # RemoveAllCollisionsFromBrace(tag_sCoasterInstance*, tag_sTrackBrace*)
  __Z30CheckAllTrackToTrackCollisionsP20tag_sCoasterInstance   # CheckAllTrackToTrackCollisions(tag_sCoasterInstance*)
  __Z30FlagRecalcAllCoasterCollisionsP20tag_sCoasterInstance   # FlagRecalcAllCoasterCollisions(tag_sCoasterInstance*)
  __Z30RemoveBraceFromCollisionSystemP15tag_sTrackBraceP20tag_sCoasterInstance   # RemoveBraceFromCollisionSystem(tag_sTrackBrace*, tag_sCoasterInstance*)
  __Z31CheckTrackToAllTracksCollisionsP20tag_sCoasterInstanceP15tag_sTrackBrace   # CheckTrackToAllTracksCollisions(tag_sCoasterInstance*, tag_sTrackBrace*)
  _gCollisionBlockA
  _gCollisionBlockB
  _gCollisionChunksA
  _gCollisionChunksB
  _gCollisionCorner0
  _gCollisionCorner1
  _gCollisionCorner2
  _gCollisionCorner3
  _gCollisionRings
  _gCollisionSampleA
  _gCollisionSampleB
  _gCollisionTypeA
  _gCollisionTypeB
)
