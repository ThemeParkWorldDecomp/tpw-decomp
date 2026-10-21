# Unit v5574: UIInterface::SetGameWindowCursor and PostLevelLoad (src/game/ui/UIInterface.cpp). The callees are
# recording fakes in tests/unit_UIInterface_SetGameWindowCursor.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIInterface_SetGameWindowCursor src/game/ui/UIInterface.cpp platform/arena.cpp)
set(TPW_UNIT_UIInterface_SetGameWindowCursor_KEEP
  __ZN11UIInterface13PostLevelLoadEv   # UIInterface::PostLevelLoad()
  __ZN11UIInterface19SetGameWindowCursorEs   # UIInterface::SetGameWindowCursor(short)
  _gUIPostLoadDate
)
