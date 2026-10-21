# Unit v5018: the static initialiser of AdvisorScoring.cpp (src/game/advisor/AdvisorScoring.cpp). The string constructor and
# __register_global_object are recording fakes in tests/unit_sinit_AdvisorScoring_cpp.cpp, which also defines the table and the sources.
set(TPW_UNIT_sinit_AdvisorScoring_cpp src/game/advisor/AdvisorScoring.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_AdvisorScoring_cpp_KEEP
  __Z20DestroyAdvisorStringPvs   # DestroyAdvisorString(void*, short)
  __Z26__sinit_AdvisorScoring_cppv   # __sinit_AdvisorScoring_cpp()
  _gAdvisorStringNode
  _gAdvisorStringStorage
)
