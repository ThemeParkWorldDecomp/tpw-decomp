# The camera's share of the game window's messages and the key tables' GetState (lead 2026-10-14).
set(TPW_UNIT_CameraMessages src/game/TPCamera.cpp src/game/keyboard.cpp)
set(TPW_UNIT_CameraMessages_KEEP
  __Z25Will_HandleCameraMessagesiii   # Will_HandleCameraMessages(int, int, int)
  __ZN9CKeyTable8GetStateEs   # CKeyTable::GetState(short)
  _gCameraMessagesF5A10Once
  _gCameraMessagesF5A14
  _gCameraYawDragScale
  _gCameraYawDragScaleOnce
  _gPointerScreenXY
)
