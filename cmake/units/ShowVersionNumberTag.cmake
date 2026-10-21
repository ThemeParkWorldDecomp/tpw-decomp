# Unit v5289: ShowVersionNumberTag and delme (src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_tags.cpp). The callees outside the
# unit are recording fakes in tests/unit_ShowVersionNumberTag.cpp.
set(TPW_UNIT_ShowVersionNumberTag src/game/ui/ui_StatusPanel.cpp src/game/ui/ui_tags.cpp platform/arena.cpp)
set(TPW_UNIT_ShowVersionNumberTag_KEEP
  __Z20ShowVersionNumberTagv   # ShowVersionNumberTag()
  __Z5delmePv   # delme(void*)
  _gVersionNumberFormat
)
