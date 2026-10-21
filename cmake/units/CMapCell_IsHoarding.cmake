# Unit v5270: CMapCell::IsHoarding, IsTrack, RefreshHFieldTile, ValidTypeChange, CalculateDirections (src/game/park/MapCell.cpp).
# The callees outside the unit are recording fakes in tests/unit_CMapCell_IsHoarding.cpp, which also defines gGameWorld.
set(TPW_UNIT_CMapCell_IsHoarding src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_IsHoarding_KEEP
  __ZN8CMapCell15ValidTypeChangeE10CELL_MODES   # CMapCell::ValidTypeChange(CELL_MODES)
  __ZN8CMapCell17RefreshHFieldTileEv   # CMapCell::RefreshHFieldTile()
  __ZN8CMapCell19CalculateDirectionsEii   # CMapCell::CalculateDirections(int, int)
  __ZNK8CMapCell10IsHoardingEv   # CMapCell::IsHoarding() const
  __ZNK8CMapCell7IsTrackEv   # CMapCell::IsTrack() const
  _gIsTrackNoParent
  _gValidTypeChangeLock
)
