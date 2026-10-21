# Unit v5895: the options panel's DoScroll and StartScroll (src/game/ui/ui_parkStatus.cpp). The callees are
# recording fakes in tests/unit_DoScroll_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_DoScroll_3 src/game/ui/ui_parkStatus.cpp platform/arena.cpp)
set(TPW_UNIT_DoScroll_3_KEEP
  __ZN13options_panel11StartScrollEii   # options_panel::StartScroll(int, int)
  __ZN13options_panel8DoScrollEii   # options_panel::DoScroll(int, int)
  __ZN14options_callee13WindowSetTextEP15InterfaceWindowPDs   # options_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN14options_callee14WindowSetValueEP15InterfaceWindowi   # options_callee::WindowSetValue(InterfaceWindow*, int)
)
