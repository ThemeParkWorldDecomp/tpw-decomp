# Unit v51031: LLCC_DrawLine, CLBGlobe::~CLBGlobe, CLBScreenComponentDownloadGlobe::WindowMessagesFn (src/game/online/lobby_globe.cpp, src/game/park/MapCell.cpp).
# The callees outside the unit are recording fakes in tests/unit_LLCC_DrawLine_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_LLCC_DrawLine_2 src/game/online/lobby_globe.cpp src/game/park/MapCell.cpp platform/arena.cpp)
set(TPW_UNIT_LLCC_DrawLine_2_KEEP
  __Z13LLCC_DrawLine10CELL_MODESiiPiS0_   # LLCC_DrawLine(CELL_MODES, int, int, int*, int*)
  __Z17LLCC_DrawLineBody10CELL_MODESiiPiS0_   # LLCC_DrawLineBody(CELL_MODES, int, int, int*, int*)
  __ZN31CLBScreenComponentDownloadGlobe16WindowMessagesFnEiii   # CLBScreenComponentDownloadGlobe::WindowMessagesFn(int, int, int)
  __ZN8CLBGlobeD1Ev   # CLBGlobe::~CLBGlobe()
)
