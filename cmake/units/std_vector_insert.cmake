# Unit v5916: std::vector<const void*> insert / erase / resize and std::__locale_imp<__int128>::~__locale_imp
# (src/game/MslSupport.cpp). The callees are recording fakes in tests/unit_std_vector_insert.cpp.
set(TPW_UNIT_std_vector_insert src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_vector_insert_KEEP
  __Z16LocaleImpDestroyP9LocaleImps   # LocaleImpDestroy(LocaleImp*, short)
  __ZN12MslPtrVector5eraseEPPKvS2_   # MslPtrVector::erase(void const**, void const**)
  __ZN12MslPtrVector6insertEPPKvjPKS1_   # MslPtrVector::insert(void const**, unsigned int, void const* const*)
  __ZN12MslPtrVector6resizeEjPKv   # MslPtrVector::resize(unsigned int, void const*)
)
