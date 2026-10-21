# Unit v51030: std::__cdeque<const void*>::insert (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std___cdeque_insert_2.cpp; IterAdd is the translation of src/game/MslSupport.cpp.
set(TPW_UNIT_std___cdeque_insert_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___cdeque_insert_2_KEEP
  __ZN13cdeque_callee6InsertEPSt8__cdequeIPKvNSt3__19allocatorIS2_EEE13MslCdequeIterjPKS2_   # cdeque_callee::Insert(std::__cdeque<void const*, std::__1::allocator<void const*>>*, MslCdequeIter, unsigned int, void const* const*)
  __ZN13cdeque_callee7IterAddEP13MslCdequeIteri   # cdeque_callee::IterAdd(MslCdequeIter*, int)
)
