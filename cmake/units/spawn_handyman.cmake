# Unit v5028: the spawn and loan key actions (src/game/keyboard.cpp). The callees are recording fakes in
# tests/unit_spawn_handyman.cpp, which also defines the globals of other files.
set(TPW_UNIT_spawn_handyman src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_spawn_handyman_KEEP
  __Z12spawn_personv   # spawn_person()
  __Z14spawn_handymanv   # spawn_handyman()
  __Z16apply_for_loan_1v   # apply_for_loan_1()
  __Z16apply_for_loan_2v   # apply_for_loan_2()
  __Z17spawn_bag_o_staffv   # spawn_bag_o_staff()
  __Z17spawn_entertainerv   # spawn_entertainer()
  __Z18spawn_five_personsv   # spawn_five_persons()
  __Z22start_winning_sequencev   # start_winning_sequence()
)
