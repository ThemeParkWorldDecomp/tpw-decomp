# Unit v5633: LLCM_MouseMoved (src/game/park/LLCellMouse.cpp). The callees outside the unit are recording fakes in
# tests/unit_LLCM_MouseMoved.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_LLCM_MouseMoved src/game/park/LLCellMouse.cpp platform/arena.cpp)
set(TPW_UNIT_LLCM_MouseMoved_KEEP
  __Z15LLCM_MouseMovediit   # LLCM_MouseMoved(int, int, unsigned short)
  _gLLCC9F34
  _gLLCC9F38
  _gLLCC9F3C
  _gLLCC9F40
  _gLLCCC20C
  _gLLCCDB38
  _gLLCCDB3C
  _gLLCCDB60
  _gLLCCDE0B4
  _gLLCCDE0B8
)
