# Unit v5920: the coaster editor's button functions (src/game/rides/CoasterEdit.cpp).
# The callees outside the unit are recording fakes in tests/unit_GetCoasterEditDefaultButtons.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetCoasterEditDefaultButtons src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_GetCoasterEditDefaultButtons_KEEP
  __Z23HandleCoasterButtonDownj   # HandleCoasterButtonDown(unsigned int)
  __Z28GetCoasterEditDefaultButtonsPj   # GetCoasterEditDefaultButtons(unsigned int*)
  __Z30GetCoasterEditAvailableButtonsPj   # GetCoasterEditAvailableButtons(unsigned int*)
)
