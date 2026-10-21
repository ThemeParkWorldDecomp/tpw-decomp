# Unit v5985: CUI2DMap::MapAreaCallback (src/game/ui/ui_2dmap.cpp).
# The callees outside the unit are recording fakes in tests/unit_CUI2DMap_MapAreaCallback.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_MapAreaCallback src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_MapAreaCallback_KEEP
  __ZN12mapcb_callee13WindowSetTextEP15InterfaceWindowPKDs   # mapcb_callee::WindowSetText(InterfaceWindow*, char16_t const*)
  __ZN8CUI2DMap15MapAreaCallbackEP15InterfaceWindowiii   # CUI2DMap::MapAreaCallback(InterfaceWindow*, int, int, int)
  __ZN8CUI2DMap21MapAreaCallbackResultEP15InterfaceWindowiii   # CUI2DMap::MapAreaCallbackResult(InterfaceWindow*, int, int, int)
  _gMapNoThingA
  _gMapNoThingB
)
