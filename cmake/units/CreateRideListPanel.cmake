# Unit v5010: the ride list panel (CreateRideListPanel; src/game/ui/ui_ridepanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateRideListPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateRideListPanel src/game/ui/ui_ridepanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateRideListPanel_KEEP
  __Z19CreateRideListPanel9UIObjType12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CreateRideListPanel(UIObjType, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  _gRideListButtonFont
  _gRideListF88
  _gRideListF8A
  _gRideListF8C
  _gRideListF90
  _gRideListNoneA
  _gRideListNoneB
  _gRideListPanel
  _gRideListScript
  _gRideListSortColumn
  _gRideListType
)
