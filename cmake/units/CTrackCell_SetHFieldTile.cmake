# Unit v5005: CTrackCell::SetHFieldTile, RefreshHFieldTile, QuickDelete (src/game/park/TrackCell.cpp). The callees outside the unit are recording fakes in
# tests/unit_CTrackCell_SetHFieldTile.cpp, which also defines the globals of other files.
set(TPW_UNIT_CTrackCell_SetHFieldTile src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CTrackCell_SetHFieldTile_KEEP
  __ZN10CTrackCell11QuickDeleteEv   # CTrackCell::QuickDelete()
  __ZN10CTrackCell13SetHFieldTileEv   # CTrackCell::SetHFieldTile()
  __ZN10CTrackCell17RefreshHFieldTileEv   # CTrackCell::RefreshHFieldTile()
  _gLLCC4840
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
