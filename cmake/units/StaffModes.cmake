# Unit w143: the park click and the staff mouse modes (src/game/Mouse.cpp, src/game/ui/ui_main.cpp, src/game/TPPointer.cpp). The landed
# src/game/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real; every other callee is a recording fake in tests/unit_StaffModes.cpp, which also
# defines the globals of other files.
set(TPW_UNIT_StaffModes src/game/Mouse.cpp src/game/ui/ui_main.cpp src/game/TPPointer.cpp)
set(TPW_UNIT_StaffModes_KEEP
  __Z17ContextMouseClickv   # ContextMouseClick()
  __Z18GetPickupWorldPosnv   # GetPickupWorldPosn()
  __ZN10CMMDefault11OnLeftClickEii   # CMMDefault::OnLeftClick(int, int)
  __ZN10CMMDefault12OnRightClickEii   # CMMDefault::OnRightClick(int, int)
  __ZN10CMMDefault7GetModeEv   # CMMDefault::GetMode()
  __ZN10CMMDefault8OnAssignEv   # CMMDefault::OnAssign()
  __ZN10CMMDefaultC1Ev   # CMMDefault::CMMDefault()
  __ZN10CMMDefaultD1Ev   # CMMDefault::~CMMDefault()
  __ZN13CMMPlaceStaff11OnLeftClickEii   # CMMPlaceStaff::OnLeftClick(int, int)
  __ZN13CMMPlaceStaff7GetModeEv   # CMMPlaceStaff::GetMode()
  __ZN13CMMPlaceStaff8OnAssignEv   # CMMPlaceStaff::OnAssign()
  __ZN13CMMPlaceStaff9OnReleaseEv   # CMMPlaceStaff::OnRelease()
  __ZN13CMMPlaceStaffC1Eihhi   # CMMPlaceStaff::CMMPlaceStaff(int, unsigned char, unsigned char, int)
  __ZN13CMMPlaceStaffD1Ev   # CMMPlaceStaff::~CMMPlaceStaff()
  __ZN14CMMPickUpStaff11OnLeftClickEii   # CMMPickUpStaff::OnLeftClick(int, int)
  __ZN14CMMPickUpStaff12OnRightClickEii   # CMMPickUpStaff::OnRightClick(int, int)
  __ZN14CMMPickUpStaff7GetModeEv   # CMMPickUpStaff::GetMode()
  __ZN14CMMPickUpStaff8OnAssignEv   # CMMPickUpStaff::OnAssign()
  __ZN14CMMPickUpStaff9OnReleaseEv   # CMMPickUpStaff::OnRelease()
  __ZN14CMMPickUpStaffC1E11ESP_Spritesh   # CMMPickUpStaff::CMMPickUpStaff(ESP_Sprites, unsigned char)
  __ZN14CMMPickUpStaffD1Ev   # CMMPickUpStaff::~CMMPickUpStaff()
  __ZN16CSpritePlaceMode11OnMouseMoveEiiii   # CSpritePlaceMode::OnMouseMove(int, int, int, int)
  __ZN16CSpritePlaceMode9OnReleaseEv   # CSpritePlaceMode::OnRelease()
  __ZN16CSpritePlaceModeD1Ev   # CSpritePlaceMode::~CSpritePlaceMode()
  __ZN6CMouse15AssignMouseModeEP10CMouseMode   # CMouse::AssignMouseMode(CMouseMode*)
  _gClickNoCell
  _gClickNoResult
  _gClickNoThing3
  _gClickNoThing5
  _gClickNoThing6
  _gMouse
  _gMouseMode
  _gMouseWord1E0
  _gMouseWord1E4
  _gStaffHQNullId
)
