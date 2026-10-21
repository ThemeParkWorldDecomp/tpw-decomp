# Unit v5259: std::list<std::string>::unique (src/game/MslSupport.cpp). The callees (string compare, list erase)
# are recording fakes in tests/unit_std_list_std_basic_string_std_allocator_std_basi_2.cpp.
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi_2_KEEP
  __Z16StringListUniquePv   # StringListUnique(void*)
)
