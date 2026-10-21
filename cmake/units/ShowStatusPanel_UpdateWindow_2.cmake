# Unit v51007: ShowStatusPanel::UpdateWindow, __sinit_ui_upgradeStatus_cpp, YearEndPanelMsg
# (src/game/ui/ui_showStatus.cpp, src/game/ui/ui_upgradeStatus.cpp, src/game/ui/ui_yearEndPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_ShowStatusPanel_UpdateWindow_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowStatusPanel_UpdateWindow_2 src/game/ui/ui_showStatus.cpp src/game/ui/ui_upgradeStatus.cpp
    src/game/ui/ui_yearEndPanel.cpp platform/arena.cpp tests/standins/ShowStatusPanel_UpdateWindow_2.cpp)
set(TPW_UNIT_ShowStatusPanel_UpdateWindow_2_KEEP
  __Z15YearEndPanelMsgP15InterfaceWindowiii   # YearEndPanelMsg(InterfaceWindow*, int, int, int)
  __Z28__sinit_ui_upgradeStatus_cppv   # __sinit_ui_upgradeStatus_cpp()
  __ZN15ShowStatusPanel12UpdateWindowEv   # ShowStatusPanel::UpdateWindow()
)
