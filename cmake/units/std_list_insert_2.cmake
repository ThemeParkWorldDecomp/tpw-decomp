# Unit v5931: std::list<unsigned int> insert, merge, remove, splice (src/game/MslSupport.cpp). The callees outside the unit are recording
# fakes in tests/unit_std_list_insert_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_list_insert_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_insert_2_KEEP
  __ZN13std_list_uint5MergeEPNS_4ListES1_   # std_list_uint::Merge(std_list_uint::List*, std_list_uint::List*)
  __ZN13std_list_uint6InsertEPjPNS_4ListEjPKj   # std_list_uint::Insert(unsigned int*, std_list_uint::List*, unsigned int, unsigned int const*)
  __ZN13std_list_uint6RemoveEPNS_4ListEPKj   # std_list_uint::Remove(std_list_uint::List*, unsigned int const*)
  __ZN13std_list_uint6SpliceEPNS_4ListEjS1_   # std_list_uint::Splice(std_list_uint::List*, unsigned int, std_list_uint::List*)
)
