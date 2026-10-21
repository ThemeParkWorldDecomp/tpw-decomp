# The park pointer (unit w142): ContextMouse and CMouse's three handlers. Every callee is a recording fake.
set(TPW_UNIT_ParkMouse src/game/ui/ui_main.cpp src/game/Mouse.cpp platform/arena.cpp)
set(TPW_UNIT_ParkMouse_KEEP
  __Z12ContextMousei   # ContextMouse(int)
  __ZN6CMouse11DoubleClickEii   # CMouse::DoubleClick(int, int)
  __ZN6CMouse18ProcessIngameMouseEiiiiPv   # CMouse::ProcessIngameMouse(int, int, int, int, void*)
  __ZN6CMouse21ImmediateMouseHandlerEii   # CMouse::ImmediateMouseHandler(int, int)
  _gMouseLeftDown
  _gMouseRightDown
)
