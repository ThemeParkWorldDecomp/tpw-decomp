# Unit v5530: Outbox::Save and Outbox::WriteMailData (src/game/online/email.cpp). The callees outside the unit are
# recording fakes in tests/unit_Outbox_WriteMailData.cpp, which also defines Outbox::Outbox.
set(TPW_UNIT_Outbox_WriteMailData src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Outbox_WriteMailData_KEEP
  __ZN6Outbox13WriteMailDataEv   # Outbox::WriteMailData()
  __ZN6Outbox4SaveEv   # Outbox::Save()
)
