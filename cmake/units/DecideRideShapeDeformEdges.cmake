# Unit v5915: UnhighlightRide, StartFlashingObject, StopFlashingObject, DecideRideShapeDeformEdges (src/game/park/SkirtDeform.cpp, src/game/rides/RidePreview.cpp).
# The callees outside the unit are recording fakes in tests/unit_DecideRideShapeDeformEdges.cpp, which also defines the globals of other files.
set(TPW_UNIT_DecideRideShapeDeformEdges src/game/park/SkirtDeform.cpp src/game/rides/RidePreview.cpp platform/arena.cpp)
set(TPW_UNIT_DecideRideShapeDeformEdges_KEEP
  __Z15UnhighlightRidev   # UnhighlightRide()
  __Z18StopFlashingObjectP16tag_sRideAnimSet   # StopFlashingObject(tag_sRideAnimSet*)
  __Z19StartFlashingObjectP16tag_sRideAnimSet   # StartFlashingObject(tag_sRideAnimSet*)
  __Z26DecideRideShapeDeformEdgesP9RideShapeii   # DecideRideShapeDeformEdges(RideShape*, int, int)
)
