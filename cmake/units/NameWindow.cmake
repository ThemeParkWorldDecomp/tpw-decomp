# Unit v5539: NameWindow and TankButton (src/game/ui/ui_researchPanel.cpp). The callees are recording fakes in tests/unit_NameWindow.cpp.
set(TPW_UNIT_NameWindow src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_NameWindow_KEEP
  __Z10NameWindowi   # NameWindow(int)
  __Z10TankButtoniP15InterfaceWindowi   # TankButton(int, InterfaceWindow*, int)
)
