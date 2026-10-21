# Unit v5671: CPerson::WinningMods (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_WinningMods.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_WinningMods src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_WinningMods_KEEP
  __ZN7CPerson11WinningModsEP7CObject   # CPerson::WinningMods(CObject*)
  _gBalloonLifeMax
  _gBalloonLifeMin
  _gWinningBalloonBlock
)
