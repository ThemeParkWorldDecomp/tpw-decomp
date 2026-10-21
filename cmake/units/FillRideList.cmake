# Unit v5363: FillRideList and UpdateViewWindow (src/game/ui/ui_ridepanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_FillRideList.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillRideList src/game/ui/ui_ridepanel.cpp platform/arena.cpp)
set(TPW_UNIT_FillRideList_KEEP
  __Z12FillRideList9UIObjTypei   # FillRideList(UIObjType, int)
  __Z16UpdateViewWindowR12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # UpdateViewWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>&)
)
