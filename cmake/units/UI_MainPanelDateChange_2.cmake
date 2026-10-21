# Unit v5995: the main panel date change, SetLastPanel, the loans panel refresh and RailsCallback (src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_mainpanel2.cpp).
# The callees outside the unit are recording fakes in tests/unit_UI_MainPanelDateChange_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_UI_MainPanelDateChange_2 src/game/ui/ui_loansPanel.cpp src/game/ui/ui_mainpanel2.cpp platform/arena.cpp)
set(TPW_UNIT_UI_MainPanelDateChange_2_KEEP
  __Z12SetLastPanelii   # SetLastPanel(int, int)
  __Z13RailsCallbackP15InterfaceWindowiii   # RailsCallback(InterfaceWindow*, int, int, int)
  __Z22UI_MainPanelDateChangeiii   # UI_MainPanelDateChange(int, int, int)
  __Z23UI_LoansPanelDateChangei   # UI_LoansPanelDateChange(int)
  __ZN12rails_callee14WindowFromWordEi   # rails_callee::WindowFromWord(int)
  __ZN12rails_callee14WindowSetValueEP15InterfaceWindowi   # rails_callee::WindowSetValue(InterfaceWindow*, int)
  __ZN12rails_callee8RectCtorEPv   # rails_callee::RectCtor(void*)
  __ZN12rails_callee8RectDtorEPv   # rails_callee::RectDtor(void*)
)
