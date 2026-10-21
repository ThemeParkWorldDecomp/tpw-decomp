# Unit v5538: EndCoasterEditMode and CoasterButtonsEnabled (src/game/ui/ui_coasterPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_EndCoasterEditMode.cpp, which also defines the globals of other files.
set(TPW_UNIT_EndCoasterEditMode src/game/ui/ui_coasterPanel.cpp platform/arena.cpp)
set(TPW_UNIT_EndCoasterEditMode_KEEP
  __Z18EndCoasterEditModev   # EndCoasterEditMode()
  __Z21CoasterButtonsEnabledj   # CoasterButtonsEnabled(unsigned int)
)
