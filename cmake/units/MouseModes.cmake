# Unit w144: the rest of Mouse.cpp (src/Mouse_modes.cpp: CMMPathDraw, CMMOnline, CMMCoaster, CMMFirstPersonTarget, __sinit_Mouse_cpp; the CMouseMode
# base bodies are inline in include/Mouse.h). The landed src/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real; every other callee is a
# recording fake in tests/unit_MouseModes.cpp, which also defines the globals of other files.
set(TPW_UNIT_MouseModes src/Mouse_modes.cpp src/Mouse.cpp platform/arena.cpp)
