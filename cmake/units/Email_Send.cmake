# Unit v5129: Email::~Email, Reset, AttachData, Send (src/game/online/EmailMessage.cpp). The callees outside the unit are recording fakes in
# tests/unit_Email_Send.cpp.
set(TPW_UNIT_Email_Send src/game/online/EmailMessage.cpp platform/arena.cpp)
set(TPW_UNIT_Email_Send_KEEP
  __ZN5Email10AttachDataEPKc   # Email::AttachData(char const*)
  __ZN5Email4SendEPKcsPcP6Outbox   # Email::Send(char const*, short, char*, Outbox*)
  __ZN5Email5ResetEv   # Email::Reset()
  __ZN5Email8DestructEPS_s   # Email::Destruct(Email*, short)
  __ZN5EmailD1Ev   # Email::~Email()
)
