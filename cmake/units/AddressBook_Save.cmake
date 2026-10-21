# Unit v5009: AddressBook::Save (src/game/online/email.cpp). The stream callees are recording fakes in
# tests/unit_AddressBook_Save.cpp.
set(TPW_UNIT_AddressBook_Save src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_AddressBook_Save_KEEP
  __Z14SaveStreamCtorP10SaveStream   # SaveStreamCtor(SaveStream*)
  __Z14SaveStreamOpenP10SaveStreamPKc   # SaveStreamOpen(SaveStream*, char const*)
  __Z15SaveStreamCloseP10SaveStream   # SaveStreamClose(SaveStream*)
  __Z15SaveStreamThrowPKc   # SaveStreamThrow(char const*)
  __Z15SaveStreamWriteP10SaveStreamPKc   # SaveStreamWrite(SaveStream*, char const*)
  __Z17SaveStreamDestroyP10SaveStream   # SaveStreamDestroy(SaveStream*)
  __ZN11AddressBook4SaveEPKc   # AddressBook::Save(char const*)
)
