# Unit v5768: ui_IME.cpp's static initialiser (src/game/ui/ui_IME.cpp). The callees outside the unit are recording fakes in
# tests/unit_sinit_ui_IME_cpp.cpp, which also defines gUIIME.
set(TPW_UNIT_sinit_ui_IME_cpp src/game/ui/ui_IME.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_ui_IME_cpp_KEEP
  __Z18__sinit_ui_IME_cppv   # __sinit_ui_IME_cpp()
  _gUIIMESinitDtorTV
  _gUIIMESinitNode
)
