# Unit v5560: the all-items list's attach function (src/ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6.cpp). The callees outside the unit are
# recording fakes in tests/unit_LCPostFormat_6.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCPostFormat_6 src/ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6.cpp platform/arena.cpp)
set_source_files_properties(src/ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6.cpp PROPERTIES COMPILE_DEFINITIONS TPW_LCPOST_FAKES)
