# Unit v5560: the all-items list's attach function (src/game/ui/ui_allItemsPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_LCPostFormat_6.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCPostFormat_6 src/game/ui/ui_allItemsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_LCPostFormat_6_KEEP
  __Z24AllItemsListLCPostFormatP15InterfaceWindow   # AllItemsListLCPostFormat(InterfaceWindow*)
)
set_source_files_properties(src/game/ui/ui_allItemsPanel.cpp PROPERTIES COMPILE_DEFINITIONS TPW_LCPOST_FAKES)
