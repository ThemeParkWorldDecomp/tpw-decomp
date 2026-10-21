# Unit v5946: LLCC_DrawTrackLine and __sinit_lobby_cpp (src/game/online/lobby.cpp, src/game/park/TrackCell.cpp). The callees
# outside the unit are recording fakes in tests/unit_LLCC_DrawTrackLine.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_DrawTrackLine src/game/online/lobby.cpp src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_DrawTrackLine_KEEP
  __Z17__sinit_lobby_cppv   # __sinit_lobby_cpp()
  __Z18LLCC_DrawTrackLine10CELL_MODESiiPiS0_   # LLCC_DrawTrackLine(CELL_MODES, int, int, int*, int*)
  _gLobbyNode487C
  _gLobbyNode48A0
  _gLobbyNode48C0
  _gLobbyWString488C
  _gLobbyWString48AC
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
