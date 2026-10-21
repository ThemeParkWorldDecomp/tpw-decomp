# Unit v5059: LLCM_CoverUpgrade and LLCM_RebuildUpgradeBridges (src/game/park/LLCellMouse.cpp). The callees outside the unit are
# recording fakes in tests/unit_LLCM_RebuildUpgradeBridges.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCM_RebuildUpgradeBridges src/game/park/LLCellMouse.cpp platform/arena.cpp)
set(TPW_UNIT_LLCM_RebuildUpgradeBridges_KEEP
  __Z17LLCM_CoverUpgradePiS_   # LLCM_CoverUpgrade(int*, int*)
  __Z26LLCM_RebuildUpgradeBridgesii   # LLCM_RebuildUpgradeBridges(int, int)
)
