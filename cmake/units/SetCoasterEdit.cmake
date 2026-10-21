# Unit v5314: SetCoasterEdit, GetPylonFromBraceMesh, GetNthBraceMesh, HandleCoasterButtonClick (src/game/rides/CoasterEdit.cpp).
# The callees outside the unit are recording fakes in tests/unit_SetCoasterEdit.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetCoasterEdit src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_SetCoasterEdit_KEEP
  __Z14SetCoasterEditPvi   # SetCoasterEdit(void*, int)
  __Z15GetNthBraceMeshP14sBraceMeshListj   # GetNthBraceMesh(sBraceMeshList*, unsigned int)
  __Z21GetPylonFromBraceMeshjPj   # GetPylonFromBraceMesh(unsigned int, unsigned int*)
  __Z24HandleCoasterButtonClickjj   # HandleCoasterButtonClick(unsigned int, unsigned int)
)
