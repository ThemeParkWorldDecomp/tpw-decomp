# Unit v5218: Outbox::Send and Outbox::NextMail (src/game/online/email.cpp). The callees outside the unit are recording fakes in
# tests/unit_Outbox_Send.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_Outbox_Send src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Outbox_Send_KEEP
  __ZN6Outbox4SendEPKcsPc   # Outbox::Send(char const*, short, char*)
  __ZN6Outbox8NextMailEP5Email   # Outbox::NextMail(Email*)
)
