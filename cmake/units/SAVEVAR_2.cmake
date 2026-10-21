# Unit SAVEVAR_2: SAVEVAR<ESP_Sprites> and __sinit_Peep_cpp (src/game/people/Peep.cpp). The callees are recording fakes in tests/unit_SAVEVAR_2.cpp.
set(TPW_UNIT_SAVEVAR_2 src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_SAVEVAR_2_KEEP
  __Z16__sinit_Peep_cppv   # __sinit_Peep_cpp()
  __Z7SAVEVARI11ESP_SpritesEiPvPT_PKci   # int SAVEVAR<ESP_Sprites>(void*, ESP_Sprites*, char const*, int)
  _gPeepC498
  _gPeepC49C
  _gPeepMapDeltaDtorVector
  _gPeepSinitNodes
  _gPeepTbErrorZero
)
