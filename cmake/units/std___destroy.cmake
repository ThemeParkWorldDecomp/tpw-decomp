# Unit v5310: std::__destroy<Argument*> / std::__destroy<Argument> (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std___destroy.cpp.
set(TPW_UNIT_std___destroy src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___destroy_KEEP
  __ZN11std_destroy10DestroyOneEP8Argument   # std_destroy::DestroyOne(Argument*)
  __ZN11std_destroy12DestroyRangeEP8ArgumentS1_   # std_destroy::DestroyRange(Argument*, Argument*)
  __ZN11std_destroy18DestroyRangeTaggedEP8ArgumentS1_   # std_destroy::DestroyRangeTagged(Argument*, Argument*)
)
