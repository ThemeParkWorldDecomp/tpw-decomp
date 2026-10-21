# Unit v5241: InitInfoWindow and UpdateInfoWindow (src/game/ui/ui_ridepanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_UpdateInfoWindow.cpp, which also defines the globals of other files.
set(TPW_UNIT_UpdateInfoWindow src/game/ui/ui_ridepanel.cpp platform/arena.cpp)
set(TPW_UNIT_UpdateInfoWindow_KEEP
  __Z14InitInfoWindowv   # InitInfoWindow()
  __Z16UpdateInfoWindowR12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # UpdateInfoWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>&)
)
