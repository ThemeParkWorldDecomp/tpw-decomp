# Unit v5012: the address book panel (CreateAddressBook, RemoveAddressBookEntry, NewEntryInAddressBook; src/game/ui/ui_Postcard.cpp).
# The callees outside the unit are recording fakes in tests/unit_CreateAddressBook.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateAddressBook src/game/ui/ui_Postcard.cpp platform/arena.cpp)
set(TPW_UNIT_CreateAddressBook_KEEP
  __Z17CreateAddressBookv   # CreateAddressBook()
  __Z21NewEntryInAddressBookv   # NewEntryInAddressBook()
  __Z22RemoveAddressBookEntryv   # RemoveAddressBookEntry()
  _gAddressBookDescription
  _gAddressBookPanel
  _gAddressEditText
  _gAddressRemoveName
)
