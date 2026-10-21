# Unit v5258: GetNodePositionSimple, SetRideSpritePosnFocus, GetMeshPtrAtHFieldCell (src/game/park/TPHField.cpp). The callees
# outside the unit are recording fakes in tests/unit_GetNodePositionSimple.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetNodePositionSimple src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_GetNodePositionSimple_KEEP
  __Z21GetNodePositionSimpleP9tag_sNodeP8s3DPointi   # GetNodePositionSimple(tag_sNode*, s3DPoint*, int)
  __Z22GetMeshPtrAtHFieldCelljj   # GetMeshPtrAtHFieldCell(unsigned int, unsigned int)
  __Z22SetRideSpritePosnFocusP9tag_sMesh   # SetRideSpritePosnFocus(tag_sMesh*)
)
