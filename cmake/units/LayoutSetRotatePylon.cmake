# Unit v5039: the pylon setters of the editor's coaster (src/game/rides/CoasterEdit.cpp). The callees outside the unit are recording fakes in
# tests/unit_LayoutSetRotatePylon.cpp, which also defines gCoasterEditInstance.
set(TPW_UNIT_LayoutSetRotatePylon src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_LayoutSetRotatePylon_KEEP
  __Z12SetLoftPylonfjii   # SetLoftPylon(float, unsigned int, int, int)
  __Z14SetRotatePylonfjii   # SetRotatePylon(float, unsigned int, int, int)
  __Z14SetWobblePylonffjii   # SetWobblePylon(float, float, unsigned int, int, int)
  __Z18LayoutSetLoftPylonfj   # LayoutSetLoftPylon(float, unsigned int)
  __Z18ToggleDrawUpSplinev   # ToggleDrawUpSpline()
  __Z20LayoutSetRotatePylonfj   # LayoutSetRotatePylon(float, unsigned int)
  __Z20LayoutSetWobblePylonffj   # LayoutSetWobblePylon(float, float, unsigned int)
  __Z21CorrectBraceNumberingP15tag_sTrackBrace   # CorrectBraceNumbering(tag_sTrackBrace*)
  _gDrawUpSplineFlag
)
