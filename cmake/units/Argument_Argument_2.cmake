# Unit v5242: Argument::Argument(const Argument&) (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_Argument_Argument_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_Argument_Argument_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_Argument_Argument_2_KEEP
  __ZN17std_vector_callee12ArgumentCopyEP8ArgumentPKS0_   # std_vector_callee::ArgumentCopy(Argument*, Argument const*)
)
