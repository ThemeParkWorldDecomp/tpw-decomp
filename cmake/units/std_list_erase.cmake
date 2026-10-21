# Unit v5305: std::list<unsigned int>::erase and clear (src/game/MslSupport.cpp). operator delete is a recording fake in tests/unit_std_list_erase.cpp.
set(TPW_UNIT_std_list_erase src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_erase_KEEP
  __ZN13std_list_uint14OperatorDeleteEPv   # std_list_uint::OperatorDelete(void*)
  __ZN13std_list_uint5ClearEPNS_4ListE   # std_list_uint::Clear(std_list_uint::List*)
  __ZN13std_list_uint5EraseEPjPNS_4ListEjj   # std_list_uint::Erase(unsigned int*, std_list_uint::List*, unsigned int, unsigned int)
)
