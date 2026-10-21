# Unit v5162: Email::SendToOutbox and Email::BuildBody (src/game/online/EmailMessage.cpp). The callees outside the unit are recording fakes in
# tests/unit_Email_SendToOutbox.cpp.
set(TPW_UNIT_Email_SendToOutbox src/game/online/EmailMessage.cpp platform/arena.cpp)
set(TPW_UNIT_Email_SendToOutbox_KEEP
  __ZN5Email12SendToOutboxEP6Outbox   # Email::SendToOutbox(Outbox*)
  __ZN5Email9BuildBodyEv   # Email::BuildBody()
)
