# Unit v5246: the coaster edit mode (CoasterEditMode, WhichSpecialButton; src/game/ui/ui_coasterPanel.cpp). The callees outside the
# unit are recording fakes in tests/unit_CoasterEditMode.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterEditMode src/game/ui/ui_coasterPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterEditMode_KEEP
  __Z15CoasterEditModePv   # CoasterEditMode(void*)
  __Z18WhichSpecialButtonv   # WhichSpecialButton()
  _gCoasterEditBusy
  _gCoasterEditObject
)
