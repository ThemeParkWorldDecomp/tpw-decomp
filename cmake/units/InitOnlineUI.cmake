# Unit v5060: InitOnlineUI and BankruptWindowMsg (src/game/ui/ui_main.cpp). The callees outside the unit are recording fakes in
# tests/unit_InitOnlineUI.cpp, which also defines the globals of other files.
set(TPW_UNIT_InitOnlineUI src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_InitOnlineUI_KEEP
  __Z12InitOnlineUIv   # InitOnlineUI()
  __Z17BankruptWindowMsgP15InterfaceWindowiii   # BankruptWindowMsg(InterfaceWindow*, int, int, int)
)
