# Unit v5086: the MSL std::list<std::string> members insert, sort, erase (two) and merge
# (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_list_std_basic_string_std_allocator_std_basi.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi_KEEP
  __ZN3IME15MslStrList_sortEPNS_10MslStrListE   # IME::MslStrList_sort(IME::MslStrList*)
  __ZN3IME16MslStrList_eraseEPNS_10MslStrListEPNS_10MslStrNodeES3_   # IME::MslStrList_erase(IME::MslStrList*, IME::MslStrNode*, IME::MslStrNode*)
  __ZN3IME16MslStrList_mergeEPNS_10MslStrListES1_   # IME::MslStrList_merge(IME::MslStrList*, IME::MslStrList*)
  __ZN3IME17MslStrList_erase1EPNS_10MslStrListEPNS_10MslStrNodeE   # IME::MslStrList_erase1(IME::MslStrList*, IME::MslStrNode*)
  __ZN3IME17MslStrList_insertEPNS_10MslStrListEPNS_10MslStrNodeEPKNS_9MslStringE   # IME::MslStrList_insert(IME::MslStrList*, IME::MslStrNode*, IME::MslString const*)
)
