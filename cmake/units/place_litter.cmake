# Unit v5299: the litter cheats place_litter and place_bomb (src/game/keyboard.cpp). The callees outside the unit are
# recording fakes in tests/unit_place_litter.cpp, which also defines the globals of other files.
set(TPW_UNIT_place_litter src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_place_litter_KEEP
  __Z10place_bombv   # place_bomb()
  __Z12place_litterv   # place_litter()
)
