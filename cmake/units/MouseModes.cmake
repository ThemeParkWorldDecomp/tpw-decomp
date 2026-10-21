# Unit w144: the rest of Mouse.cpp (src/game/Mouse.cpp: CMMPathDraw, CMMOnline, CMMCoaster, CMMFirstPersonTarget, __sinit_Mouse_cpp; the CMouseMode
# base bodies are inline in include/Mouse.h). The landed src/game/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real; every other callee is a
# recording fake in tests/unit_MouseModes.cpp, which also defines the globals of other files.
set(TPW_UNIT_MouseModes src/game/Mouse.cpp platform/arena.cpp)
set(TPW_UNIT_MouseModes_KEEP
  __ZN10CMMCoaster11OnLeftClickEii   # CMMCoaster::OnLeftClick(int, int)
  __ZN10CMMCoaster12OnRightClickEii   # CMMCoaster::OnRightClick(int, int)
  __ZN10CMMCoaster13OnLeftReleaseEii   # CMMCoaster::OnLeftRelease(int, int)
  __ZN10CMMCoaster7GetModeEv   # CMMCoaster::GetMode()
  __ZN10CMMCoaster8OnAssignEv   # CMMCoaster::OnAssign()
  __ZN10CMMCoasterC1Ev   # CMMCoaster::CMMCoaster()
  __ZN10CMMCoasterD1Ev   # CMMCoaster::~CMMCoaster()
  __ZN10CMMDefault11OnLeftClickEii   # CMMDefault::OnLeftClick(int, int)
  __ZN10CMMDefault12OnRightClickEii   # CMMDefault::OnRightClick(int, int)
  __ZN10CMMDefault7GetModeEv   # CMMDefault::GetMode()
  __ZN10CMMDefault8OnAssignEv   # CMMDefault::OnAssign()
  __ZN10CMMDefaultC1Ev   # CMMDefault::CMMDefault()
  __ZN10CMMDefaultD1Ev   # CMMDefault::~CMMDefault()
  __ZN11CMMPathDraw11OnLeftClickEii   # CMMPathDraw::OnLeftClick(int, int)
  __ZN11CMMPathDraw11OnMouseMoveEiiii   # CMMPathDraw::OnMouseMove(int, int, int, int)
  __ZN11CMMPathDraw12OnRightClickEii   # CMMPathDraw::OnRightClick(int, int)
  __ZN11CMMPathDraw13OnLeftReleaseEii   # CMMPathDraw::OnLeftRelease(int, int)
  __ZN11CMMPathDraw13WhileLeftDownEii   # CMMPathDraw::WhileLeftDown(int, int)
  __ZN11CMMPathDraw14WhileRightDownEii   # CMMPathDraw::WhileRightDown(int, int)
  __ZN11CMMPathDraw7GetModeEv   # CMMPathDraw::GetMode()
  __ZN11CMMPathDraw8OnAssignEv   # CMMPathDraw::OnAssign()
  __ZN11CMMPathDrawC1E10CELL_MODES   # CMMPathDraw::CMMPathDraw(CELL_MODES)
  __ZN11CMMPathDrawC1E10CELL_MODES12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CMMPathDraw::CMMPathDraw(CELL_MODES, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN11CMMPathDrawD1Ev   # CMMPathDraw::~CMMPathDraw()
  __ZN20CMMFirstPersonTarget11OnLeftClickEii   # CMMFirstPersonTarget::OnLeftClick(int, int)
  __ZN20CMMFirstPersonTarget11OnMouseMoveEiiii   # CMMFirstPersonTarget::OnMouseMove(int, int, int, int)
  __ZN20CMMFirstPersonTarget12OnRightClickEii   # CMMFirstPersonTarget::OnRightClick(int, int)
  __ZN20CMMFirstPersonTarget7GetModeEv   # CMMFirstPersonTarget::GetMode()
  __ZN20CMMFirstPersonTarget8OnAssignEv   # CMMFirstPersonTarget::OnAssign()
  __ZN20CMMFirstPersonTargetC1Ev   # CMMFirstPersonTarget::CMMFirstPersonTarget()
  __ZN20CMMFirstPersonTargetD1Ev   # CMMFirstPersonTarget::~CMMFirstPersonTarget()
  __ZN6CMouse15AssignMouseModeEP10CMouseMode   # CMouse::AssignMouseMode(CMouseMode*)
  __ZN9CMMOnline11OnLeftClickEii   # CMMOnline::OnLeftClick(int, int)
  __ZN9CMMOnline12OnRightClickEii   # CMMOnline::OnRightClick(int, int)
  __ZN9CMMOnline13OnDoubleClickEii   # CMMOnline::OnDoubleClick(int, int)
  __ZN9CMMOnline7GetModeEv   # CMMOnline::GetMode()
  __ZN9CMMOnline8OnAssignEv   # CMMOnline::OnAssign()
  __ZN9CMMOnlineC1Ev   # CMMOnline::CMMOnline()
  __ZN9CMMOnlineD1Ev   # CMMOnline::~CMMOnline()
  _gMouse
  _gMouseMode
  _gMouseWord1E0
  _gMouseWord1E4
  _gMouseWord1F8
  _gMouseWord1FC
  _gOverRideNullId
  _gPointerNullId
)
