# Unit v5683: std::list<unsigned int>::unique and ::sort (src/game/MslSupport.cpp). Merge and operator delete are fakes in tests/unit_std_list_unique.cpp.
set(TPW_UNIT_std_list_unique src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_unique_KEEP
  __ZN13std_list_uint4SortEPNS_4ListE   # std_list_uint::Sort(std_list_uint::List*)
  __ZN13std_list_uint6UniqueEPNS_4ListE   # std_list_uint::Unique(std_list_uint::List*)
)
