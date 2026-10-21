# Unit v5181: std::vector<Argument> copy constructor and push_back (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_vector_vector.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_vector_vector src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_vector_vector_KEEP
  __ZN14ArgumentVector9push_backERK8Argument   # ArgumentVector::push_back(Argument const&)
  __ZN14ArgumentVectorC1ERKS_   # ArgumentVector::ArgumentVector(ArgumentVector const&)
)
