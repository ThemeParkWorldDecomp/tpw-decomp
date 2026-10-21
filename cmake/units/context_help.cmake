# Unit v51122: the keyboard action wrappers, context_help, the Remove*Panel functions, GameWindowToggle_Hidden and
# __sinit_ui_mainpanel2_cpp (src/game/keyboard.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_mainpanel2.cpp, src/game/ui/ui_optionsPanel.cpp, src/game/ui/ui_parkStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_context_help.cpp, which also defines the globals of other files.
set(TPW_UNIT_context_help src/game/keyboard.cpp src/game/ui/ui_Postcard.cpp src/game/ui/ui_loansPanel.cpp
    src/game/ui/ui_mainpanel2.cpp src/game/ui/ui_optionsPanel.cpp src/game/ui/ui_parkStatus.cpp platform/arena.cpp)
set(TPW_UNIT_context_help_KEEP
  __Z10Screen_Mapv   # Screen_Map()
  __Z12Screen_Loansv   # Screen_Loans()
  __Z12context_helpv   # context_help()
  __Z12show_versionv   # show_version()
  __Z13CamcorderModev   # CamcorderMode()
  __Z14Screen_Financev   # Screen_Finance()
  __Z14Screen_PeepLocv   # Screen_PeepLoc()
  __Z14ToggleHelpTextv   # ToggleHelpText()
  __Z15Screen_AllItemsv   # Screen_AllItems()
  __Z15Screen_AllPeepsv   # Screen_AllPeeps()
  __Z15Screen_AllStaffv   # Screen_AllStaff()
  __Z15Screen_BuyStaffv   # Screen_BuyStaff()
  __Z15Screen_Researchv   # Screen_Research()
  __Z15Screen_StaffLocv   # Screen_StaffLoc()
  __Z16RemoveLoansPanelv   # RemoveLoansPanel()
  __Z17RemoveAddressBookv   # RemoveAddressBook()
  __Z17Screen_EntryPricev   # Screen_EntryPrice()
  __Z17Screen_ParkStatusv   # Screen_ParkStatus()
  __Z17Screen_StaffCostsv   # Screen_StaffCosts()
  __Z18RemoveOptionsPanelv   # RemoveOptionsPanel()
  __Z21RemoveParkStatusPanelv   # RemoveParkStatusPanel()
  __Z23GameWindowToggle_Hiddenv   # GameWindowToggle_Hidden()
  __Z25__sinit_ui_mainpanel2_cppv   # __sinit_ui_mainpanel2_cpp()
  __Z30MainPanel_EnableResearchButtonb   # MainPanel_EnableResearchButton(bool)
  _gMainPanel2Literal
)
