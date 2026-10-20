# Unit v5218: Outbox::Send and Outbox::NextMail (src/email_load_Outbox_Send.cpp). The callees outside the unit are recording fakes in
# tests/unit_Outbox_Send.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_Outbox_Send src/email_load_Outbox_Send.cpp platform/arena.cpp)
