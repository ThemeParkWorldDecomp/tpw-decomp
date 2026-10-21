# Unit v51111: small members of the interface controls and panels (src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceImage.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceLightPanel.cpp, src/game/ui/InterfaceTextBox.cpp, src/game/ui/InterfaceVaryBox.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_progressBar.cpp, src/game/ui/ui_upgradeStatus.cpp, src/game/ui/ui_yearEndPanel.cpp). The callees outside
# the unit are recording fakes in tests/unit_InterfaceKnob_GetValue.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceKnob_GetValue src/game/ui/InterfaceBar.cpp src/game/ui/InterfaceImage.cpp
    src/game/ui/InterfaceKnob.cpp src/game/ui/InterfaceLightPanel.cpp src/game/ui/InterfaceTextBox.cpp
    src/game/ui/InterfaceVaryBox.cpp src/game/ui/InterfaceWindow.cpp src/game/ui/UI_Base.cpp
    src/game/ui/ui_progressBar.cpp src/game/ui/ui_upgradeStatus.cpp src/game/ui/ui_yearEndPanel.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceKnob_GetValue_KEEP
  __Z10ProgBarMsgP15InterfaceWindowiii   # ProgBarMsg(InterfaceWindow*, int, int, int)
  __Z18RemoveYearEndPanelv   # RemoveYearEndPanel()
  __Z18UI_HelpTextEnabledv   # UI_HelpTextEnabled()
  __ZN12InterfaceBar8GetValueEv   # InterfaceBar::GetValue()
  __ZN13InterfaceKnob8GetValueEv   # InterfaceKnob::GetValue()
  __ZN13InterfaceKnob8SetValueEi   # InterfaceKnob::SetValue(int)
  __ZN13InterfaceKnobD1Ev   # InterfaceKnob::~InterfaceKnob()
  __ZN14InterfaceImage8GetValueEv   # InterfaceImage::GetValue()
  __ZN15InterfaceWindow11CaptureKeysEP14captureKeyList   # InterfaceWindow::CaptureKeys(captureKeyList*)
  __ZN15InterfaceWindow13stopAnimTimerEi   # InterfaceWindow::stopAnimTimer(int)
  __ZN16InterfaceTextBox12DrawFunctionEv   # InterfaceTextBox::DrawFunction()
  __ZN16InterfaceVaryBox6SetIncEPi   # InterfaceVaryBox::SetInc(int*)
  __ZN16InterfaceVaryBox8GetValueEv   # InterfaceVaryBox::GetValue()
  __ZN16InterfaceVaryBox8SetValueEi   # InterfaceVaryBox::SetValue(int)
  __ZN16InterfaceVaryBoxD1Ev   # InterfaceVaryBox::~InterfaceVaryBox()
  __ZN16InterfaceVaryBoxmiER15InterfaceWindow   # InterfaceVaryBox::operator-(InterfaceWindow&)
  __ZN18UpgradeStatusPanel12DeInitWindowEv   # UpgradeStatusPanel::DeInitWindow()
  __ZN18UpgradeStatusPanel12UpdateWindowEv   # UpgradeStatusPanel::UpdateWindow()
  __ZN18UpgradeStatusPanel18DeInitStatusWindowEv   # UpgradeStatusPanel::DeInitStatusWindow()
  __ZN18UpgradeStatusPanel6RemoveEv   # UpgradeStatusPanel::Remove()
  __ZN19InterfaceLightPanel8GetValueEv   # InterfaceLightPanel::GetValue()
  __ZN19InterfaceLightPanelD1Ev   # InterfaceLightPanel::~InterfaceLightPanel()
)
