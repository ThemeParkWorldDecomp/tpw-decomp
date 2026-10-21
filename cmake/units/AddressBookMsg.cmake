# Unit v5055: the address book panel's message function and list filler (AddressBookMsg, FillAddressList; src/game/ui/ui_Postcard.cpp).
# The callees outside the unit are recording fakes in tests/unit_AddressBookMsg.cpp, which also defines the globals of other files.
set(TPW_UNIT_AddressBookMsg src/game/ui/ui_Postcard.cpp platform/arena.cpp)
set(TPW_UNIT_AddressBookMsg_KEEP
  __Z14AddressBookMsgP15InterfaceWindowiii   # AddressBookMsg(InterfaceWindow*, int, int, int)
  __Z15FillAddressListi   # FillAddressList(int)
)
