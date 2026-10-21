# Unit v5631: CAdvisor::ReceiveMessage (0x1000c874; src/game/advisor/Advisor.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisor_ReceiveMessage.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisor_ReceiveMessage src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisor_ReceiveMessage_KEEP
  __ZN8CAdvisor14ReceiveMessageERK4CMsg   # CAdvisor::ReceiveMessage(CMsg const&)
)
