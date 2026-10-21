# Unit v5707: UIHandleEndLoop, UIHandleMidLoop, SetPylonType (src/game/rides/CoasterEdit.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIHandleEndLoop.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIHandleEndLoop src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_UIHandleEndLoop_KEEP
  __Z12SetPylonTypej   # SetPylonType(unsigned int)
  __Z15UIHandleEndLoopv   # UIHandleEndLoop()
  __Z15UIHandleMidLoopv   # UIHandleMidLoop()
)
