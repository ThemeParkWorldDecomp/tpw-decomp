# Unit v5111: TestAdvisor2/3/4 and delete_ride (src/game/keyboard.cpp). Callees are recording fakes in tests/unit_TestAdvisor3.cpp.
set(TPW_UNIT_TestAdvisor3 src/game/keyboard.cpp platform/arena.cpp)
set(TPW_UNIT_TestAdvisor3_KEEP
  __Z11delete_ridev   # delete_ride()
  __Z12TestAdvisor2v   # TestAdvisor2()
  __Z12TestAdvisor3v   # TestAdvisor3()
  __Z12TestAdvisor4v   # TestAdvisor4()
)
