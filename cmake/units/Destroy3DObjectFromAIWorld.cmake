# Unit v5901: the AI-world cell helpers (src/game/park/TPHField.cpp). The callees outside the unit are recording fakes in
# tests/unit_Destroy3DObjectFromAIWorld.cpp, which also defines the globals of other files.
set(TPW_UNIT_Destroy3DObjectFromAIWorld src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_Destroy3DObjectFromAIWorld_KEEP
  __Z15GetRideIDAtCellii   # GetRideIDAtCell(int, int)
  __Z21GetWorldGSquareHeightiiPj   # GetWorldGSquareHeight(int, int, unsigned int*)
  __Z23Assign3DObjectToAIWorldPvjjji   # Assign3DObjectToAIWorld(void*, unsigned int, unsigned int, unsigned int, int)
  __Z24DetermineInnerBaseInsetsP8s2DPointP8s3DPointiS0_   # DetermineInnerBaseInsets(s2DPoint*, s3DPoint*, int, s2DPoint*)
  __Z26Destroy3DObjectFromAIWorldjj   # Destroy3DObjectFromAIWorld(unsigned int, unsigned int)
  _gLevelNoThingId
)
