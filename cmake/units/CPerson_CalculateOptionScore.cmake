# Unit v5686: CPerson::CalculateOptionScore (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_CalculateOptionScore.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_CalculateOptionScore src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_CalculateOptionScore_KEEP
  __ZN7CPerson20CalculateOptionScoreEP7CObject   # CPerson::CalculateOptionScore(CObject*)
  _gAmountMatchTable
  _gNeedMatchTable
  _gNoBackOfQueue
  _gNoThingRide
)
