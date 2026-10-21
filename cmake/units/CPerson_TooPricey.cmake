# Unit v5238: CPerson::TooPricey and CPerson::ExtraExcitementEffects (src/game/people/Person.cpp). The callees
# outside the unit are recording fakes in tests/unit_CPerson_TooPricey.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_TooPricey src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_TooPricey_KEEP
  __ZN7CPerson22ExtraExcitementEffectsEPK7CObject   # CPerson::ExtraExcitementEffects(CObject const*)
  __ZNK7CPerson9TooPriceyEP7CObject   # CPerson::TooPricey(CObject*) const
)
