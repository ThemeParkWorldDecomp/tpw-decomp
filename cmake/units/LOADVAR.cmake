# Unit v5315: LOADVAR<CPerson::PersonState> (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_LOADVAR.cpp.
set(TPW_UNIT_LOADVAR src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_LOADVAR_KEEP
  __Z7LOADVARIN7CPerson11PersonStateEEiPvPT_PKci   # int LOADVAR<CPerson::PersonState>(void*, CPerson::PersonState*, char const*, int)
)
