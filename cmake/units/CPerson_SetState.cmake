# Unit v5370: CPerson::SetState (src/game/people/PersonQueue.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_SetState.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_SetState src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_SetState_KEEP
  __ZN7CPerson8SetStateENS_11PersonStateE   # CPerson::SetState(CPerson::PersonState)
  _gPersonScriptBlock
)
