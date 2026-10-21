# Unit v5972: CPerson::Serialise (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_Serialise.cpp.
set(TPW_UNIT_CPerson_Serialise src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_Serialise_KEEP
  __ZN7CPerson9SerialiseEPviij   # CPerson::Serialise(void*, int, int, unsigned int)
)
