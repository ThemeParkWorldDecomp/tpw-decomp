# Unit v5904: FeatureStatusMsg, __sinit_ui_FeatureStatus_cpp, GetMaxYValue, KissMyLines (src/game/ui/ui_FeatureStatus.cpp, src/game/ui/ui_FinancePanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_FeatureStatusMsg_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_FeatureStatusMsg_2 src/game/ui/ui_FeatureStatus.cpp src/game/ui/ui_FinancePanel.cpp platform/arena.cpp)
set(TPW_UNIT_FeatureStatusMsg_2_KEEP
  __Z11KissMyLinesR10CUIGrapher   # KissMyLines(CUIGrapher&)
  __Z12GetMaxYValuev   # GetMaxYValue()
  __Z16FeatureStatusMsgP15InterfaceWindowiii   # FeatureStatusMsg(InterfaceWindow*, int, int, int)
  __Z28__sinit_ui_FeatureStatus_cppv   # __sinit_ui_FeatureStatus_cpp()
)
