# Unit v51109: twenty small panel functions (src/game/ui/ui_FinancePanel.cpp, src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp, src/game/ui/ui_allStaffPanel.cpp, src/game/ui/ui_coasterPanel.cpp, src/game/ui/ui_controlPanel.cpp, src/game/ui/ui_entryPricePanel.cpp, src/game/ui/ui_hirePanel.cpp, src/game/ui/ui_loadSavePanel.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_textmanager.cpp). The callees outside the unit are recording fakes in
# tests/unit_FillAllRidesListCallback.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillAllRidesListCallback src/game/ui/ui_FinancePanel.cpp src/game/ui/ui_StatusPanel.cpp
    src/game/ui/ui_allItemsPanel.cpp src/game/ui/ui_allKidsPanel.cpp src/game/ui/ui_allStaffPanel.cpp
    src/game/ui/ui_coasterPanel.cpp src/game/ui/ui_controlPanel.cpp src/game/ui/ui_entryPricePanel.cpp
    src/game/ui/ui_hirePanel.cpp src/game/ui/ui_loadSavePanel.cpp src/game/ui/ui_loansPanel.cpp
    src/game/ui/ui_textmanager.cpp platform/arena.cpp)
set(TPW_UNIT_FillAllRidesListCallback_KEEP
  __Z12GetMinYValuev   # GetMinYValue()
  __Z13PaybackLoanOKPv   # PaybackLoanOK(void*)
  __Z15RemoveLoadPanelv   # RemoveLoadPanel()
  __Z15RemoveSavePanelv   # RemoveSavePanel()
  __Z16ShowControlPanelv   # ShowControlPanel()
  __Z18CoasterButtonsDownj   # CoasterButtonsDown(unsigned int)
  __Z18RemoveAllKidsPanelv   # RemoveAllKidsPanel()
  __Z18RemoveControlPanelv   # RemoveControlPanel()
  __Z19RemoveAllItemsPanelv   # RemoveAllItemsPanel()
  __Z19RemoveAllStaffPanelv   # RemoveAllStaffPanel()
  __Z19RemoveHireListPanelv   # RemoveHireListPanel()
  __Z21RemoveEntryPricePanelv   # RemoveEntryPricePanel()
  __Z23RemoveFinancePanelPanelv   # RemoveFinancePanelPanel()
  __Z24FillAllRidesListCallbackP6CThing   # FillAllRidesListCallback(CThing*)
  __Z26__sinit_ui_StatusPanel_cppv   # __sinit_ui_StatusPanel_cpp()
  __Z26__sinit_ui_textmanager_cppv   # __sinit_ui_textmanager_cpp()
  __Z27StatusPanelIteratorCallbackP6CThing   # StatusPanelIteratorCallback(CThing*)
  __Z27__sinit_ui_allKidsPanel_cppv   # __sinit_ui_allKidsPanel_cpp()
  __Z28__sinit_ui_allStaffPanel_cppv   # __sinit_ui_allStaffPanel_cpp()
  __Z30__sinit_ui_entryPricePanel_cppv   # __sinit_ui_entryPricePanel_cpp()
  _gAllKidsWord47cc
  _gAllKidsWord47d0
  _gAllStaffWord47f4
  _gAllStaffWord47f8
  _gCoolRectDtor
  _gCoolRectNode
  _gEntryPriceStringNode
  _gEntryPriceStringStorage
  _gStatusPanelWord47a8
  _gStatusPanelWord47ac
)
