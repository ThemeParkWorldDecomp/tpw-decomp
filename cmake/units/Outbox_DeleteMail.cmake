# Unit v5690: Outbox::DeleteMail (src/game/online/email.cpp). unlink is a recording fake in
# tests/unit_Outbox_DeleteMail.cpp.
set(TPW_UNIT_Outbox_DeleteMail src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Outbox_DeleteMail_KEEP
  __ZN6Outbox10DeleteMailEi   # Outbox::DeleteMail(int)
)
