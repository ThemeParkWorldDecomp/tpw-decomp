# Unit v5722: AddressBook::Add and AddressBook::Remove (src/game/online/email.cpp). The list callees are
# modelled in the test (the PowerPC side) and by the host's std::list.
set(TPW_UNIT_AddressBook_Add src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_AddressBook_Add_KEEP
  __Z19AddressBookAddIndexP11AddressBookRKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE   # AddressBookAddIndex(AddressBook*, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&)
  __ZN11AddressBook3AddERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE   # AddressBook::Add(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&)
  __ZN11AddressBook6RemoveEi   # AddressBook::Remove(int)
)
