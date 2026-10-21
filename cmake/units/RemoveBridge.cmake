# Unit v5689: RemoveBridge and LLCC_SystemGetPointedPeepThing (src/game/park/LLCellSystem.cpp, src/game/park/MapCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_RemoveBridge.cpp, which also defines the globals of other files.
set(TPW_UNIT_RemoveBridge src/game/park/LLCellSystem.cpp src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_RemoveBridge_KEEP
  __Z12RemoveBridgeP10CTrackCell8MapDeltaS1_   # RemoveBridge(CTrackCell*, MapDelta, MapDelta)
  __Z30LLCC_SystemGetPointedPeepThingv   # LLCC_SystemGetPointedPeepThing()
)
