# Unit v5022: the congratulations and bankrupt windows of the main app (UI_Congratulations, UI_Bankrupt, CongratsWindowMsg,
# __sinit_ui_main_cpp; src/game/ui/ui_main.cpp). The callees outside the unit are recording fakes in
# tests/unit_UI_Congratulations.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_UI_Congratulations src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_UI_Congratulations_KEEP
  __Z11UI_BankruptP7CObject   # UI_Bankrupt(CObject*)
  __Z17CongratsWindowMsgP15InterfaceWindowiii   # CongratsWindowMsg(InterfaceWindow*, int, int, int)
  __Z18UI_CongratulationsP7CObject   # UI_Congratulations(CObject*)
  __Z19UIMain_DestroyLightPv   # UIMain_DestroyLight(void*)
  __Z19__sinit_ui_main_cppv   # __sinit_ui_main_cpp()
  __Z20UIMain_DestroyStringPv   # UIMain_DestroyString(void*)
  __Z25UIMain_DestroyCursorTablePv   # UIMain_DestroyCursorTable(void*)
  _gBankruptAnim
  _gBankruptScript
  _gCongratsAnim
  _gCongratsScript
  _gCongratsTicks
  _gUIMainNodeButton
  _gUIMainNodeCursors
  _gUIMainNodeFrame
  _gUIMainNodeLight
  _gUIMainNodePlain
  _gUIMainNodeSlider
  _gUIMainWordCCF8
  _gUIMainWordCCFC
)
