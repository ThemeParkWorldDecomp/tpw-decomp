# Unit v5526: CUI2DMap::MapCallback (src/game/ui/ui_2dmap.cpp). The callees
# outside the unit are recording fakes in tests/unit_CUI2DMap_MapCallback.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_MapCallback src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMap_MapCallback_KEEP
  __ZN8CUI2DMap11MapCallbackEP15InterfaceWindowiii   # CUI2DMap::MapCallback(InterfaceWindow*, int, int, int)
)
