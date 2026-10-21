# Unit v5134: ui_ErrorDisplay.cpp's static initialiser (src/game/ui/ui_ErrorDisplay.cpp). The callees are recording fakes in
# tests/unit_sinit_ui_ErrorDisplay_cpp.cpp, which also defines the globals of other files.
set(TPW_UNIT_sinit_ui_ErrorDisplay_cpp src/game/ui/ui_ErrorDisplay.cpp platform/arena.cpp
    tests/standins/sinit_ui_ErrorDisplay_cpp.cpp)
set(TPW_UNIT_sinit_ui_ErrorDisplay_cpp_KEEP
  __Z27__sinit_ui_ErrorDisplay_cppv   # __sinit_ui_ErrorDisplay_cpp()
  __ZN15errsinit_callee16BthreadMutexCtorEPv   # errsinit_callee::BthreadMutexCtor(void*)
  _gSinitErrorMutexDtor
  _gSinitErrorMutexNode
  _gSinitErrorQueueDtor
  _gSinitErrorQueueNode
)
