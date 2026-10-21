# Unit v5979: HideIfUnder and AerialCallback (src/game/ui/ui_mainpanel2.cpp). The callees outside the unit are recording fakes in
# tests/unit_HideIfUnder.cpp, which also defines the globals of other files.
set(TPW_UNIT_HideIfUnder src/game/ui/ui_mainpanel2.cpp platform/arena.cpp)
set(TPW_UNIT_HideIfUnder_KEEP
  __Z11HideIfUnderP15InterfaceWindows   # HideIfUnder(InterfaceWindow*, short)
  __Z14AerialCallbackP15InterfaceWindowiii   # AerialCallback(InterfaceWindow*, int, int, int)
  _gAerialSlide
)
