# Unit w185: the cell system's modes, ride words, costs, cursor, click stack and first block fix up (src/game/park/LLCellSystem.cpp). The cost words and
# the click stack count come from the two small landed files; the landed src/game/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real. Every other
# callee is a recording fake in tests/unit_LLCellModes.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCellModes src/game/park/LLCellSystem.cpp src/game/Mouse.cpp platform/arena.cpp)
set(TPW_UNIT_LLCellModes_KEEP
  __Z17LLCC_SystemIsModei   # LLCC_SystemIsMode(int)
  __Z18LLCC_FixFirstBlockb   # LLCC_FixFirstBlock(bool)
  __Z18LLCC_SystemSetModeib   # LLCC_SystemSetMode(int, bool)
  __Z18LLCC_SystemSetRideRK12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEi   # LLCC_SystemSetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short> const&, int)
  __Z20LLCC_GetRideRotationv   # LLCC_GetRideRotation()
  __Z20LLCC_SetRideRotationib   # LLCC_SetRideRotation(int, bool)
  __Z20LLCC_SystemSetCursorv   # LLCC_SystemSetCursor()
  __Z20LLCC_SystemSetModeNRib   # LLCC_SystemSetModeNR(int, bool)
  __Z21LLCC_ResetBuyLandListv   # LLCC_ResetBuyLandList()
  __Z22LLCC_SystemGetLastModev   # LLCC_SystemGetLastMode()
  __Z22LLCC_SystemGetPathCostv   # LLCC_SystemGetPathCost()
  __Z22LLCC_SystemSetPathCosti   # LLCC_SystemSetPathCost(int)
  __Z23LLCC_SystemGetQueueCostv   # LLCC_SystemGetQueueCost()
  __Z23LLCC_SystemGetRideQueuev   # LLCC_SystemGetRideQueue()
  __Z23LLCC_SystemGetRideTrackv   # LLCC_SystemGetRideTrack()
  __Z23LLCC_SystemSetQueueCosti   # LLCC_SystemSetQueueCost(int)
  __Z23LLCC_SystemSetRideQueuei   # LLCC_SystemSetRideQueue(int)
  __Z23LLCC_SystemSetRideTracki   # LLCC_SystemSetRideTrack(int)
  __Z25LLCC_SystemGetSessionCostv   # LLCC_SystemGetSessionCost()
  __Z25LLCC_SystemSetSessionCosti   # LLCC_SystemSetSessionCost(int)
  __Z26LLCC_SystemAddToClickStackii   # LLCC_SystemAddToClickStack(int, int)
  __Z26LLCC_SystemResetClickStackv   # LLCC_SystemResetClickStack()
  __Z27LLCC_SystemAddToSessionCosti   # LLCC_SystemAddToSessionCost(int)
  __Z27LLCC_SystemGetKartTrackCostv   # LLCC_SystemGetKartTrackCost()
  __Z27LLCC_SystemSetKartTrackCosti   # LLCC_SystemSetKartTrackCost(int)
  __Z28LLCC_SystemGetWaterTrackCostv   # LLCC_SystemGetWaterTrackCost()
  __Z28LLCC_SystemSetWaterTrackCosti   # LLCC_SystemSetWaterTrackCost(int)
  __Z29LLCC_SystemMakeClickStackCopyv   # LLCC_SystemMakeClickStackCopy()
  __Z31LLCC_SystemGetClickStackEntriesv   # LLCC_SystemGetClickStackEntries()
  __ZN10CMMDefault11OnLeftClickEii   # CMMDefault::OnLeftClick(int, int)
  __ZN10CMMDefault12OnRightClickEii   # CMMDefault::OnRightClick(int, int)
  __ZN10CMMDefault7GetModeEv   # CMMDefault::GetMode()
  __ZN10CMMDefault8OnAssignEv   # CMMDefault::OnAssign()
  __ZN10CMMDefaultC1Ev   # CMMDefault::CMMDefault()
  __ZN10CMMDefaultD1Ev   # CMMDefault::~CMMDefault()
  __ZN6CMouse15AssignMouseModeEP10CMouseMode   # CMouse::AssignMouseMode(CMouseMode*)
  _gLLCC47D0
  _gLLCC47D4
  _gLLCC47D8
  _gLLCC47DC
  _gLLCC9F14
  _gLLCC9F24
  _gLLCC9F26
  _gLLCCClickCopyCount
  _gLLCCClickCopyIndex
  _gLLCCClickStack
  _gLLCCClickStackCopy
  _gLLCCClickStackCount
  _gLLCCDB58
  _gLLCCDB5A
  _gLLCCE0DC
  _gLLCCE0E0
  _gLLCCE288
  _gLLCCE28C
  _gLLCCFirstBlockX
  _gLLCCFirstBlockY
  _gMouse
  _gMouseMode
  _gMouseWord1E0
  _gMouseWord1E4
)
