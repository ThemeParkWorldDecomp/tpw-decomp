# Unit v5531: OverrideTag and TagDlgCallback (src/game/ui/ui_tags.cpp). The callees outside the unit are
# recording fakes in tests/unit_OverrideTag.cpp, which also defines the globals of other files.
set(TPW_UNIT_OverrideTag src/game/ui/ui_tags.cpp platform/arena.cpp)
set(TPW_UNIT_OverrideTag_KEEP
  __Z11OverrideTagR10CTagParams   # OverrideTag(CTagParams&)
  __Z14TagDlgCallbackP15InterfaceWindowiii   # TagDlgCallback(InterfaceWindow*, int, int, int)
  _gTagPanelPending
  _gTagShiftAnim
  _gTagSlideAnim
)
